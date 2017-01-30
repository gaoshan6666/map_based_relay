#include "pkt_output.h"
#include "debug.h"
#include <linux/skbuff.h>
#include <linux/ip.h>

extern int debug_level;

int push_relay_mac(struct sk_buff *skb, const void *addr)
{
	int head_need = ETH_ALEN - skb_headroom(skb);

	if (head_need > 0) {
		skb_orphan(skb); //I don't know if this line is expendable.
		if (pskb_expand_head(skb, head_need, 0, GFP_ATOMIC))
			return -ENOMEM;
		skb->truesize += head_need;
	}

	memcpy(skb_push(skb, ETH_ALEN), addr, ETH_ALEN);
	skb_pull(skb, ETH_ALEN);//Restore the skb point.
	return 0;
}
int eth_header_dirty(struct sk_buff *skb, unsigned short type,
	       const void *daddr, const void *saddr, unsigned int len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb, ETH_HLEN);

	if(eth == NULL) {
		return -1;
	}
	if (type != ETH_P_802_3 && type != ETH_P_802_2)
		eth->h_proto = htons(type);
	else
		eth->h_proto = htons(len);

	/*
	 *      Set the source hardware address.
	 */

	if (saddr)
		memcpy(eth->h_source, saddr, ETH_ALEN);

	if (daddr) {
		memcpy(eth->h_dest, daddr, ETH_ALEN);

	}
	return ETH_HLEN;
}


unsigned int output_handler(const struct nf_hook_ops *ops, struct sk_buff *skb,
		const struct net_device *in, const struct net_device *out, int (*okfn) (struct sk_buff *))
{
	int err;
	//mbr_dbg(debug_level, ANY, "")
#define GEL01 {0x54,0x27,0x1e,0xa4,0xca,0xe3}
#define GEL02 {0x54,0x27,0x1e,0x1a,0x77,0x99}
#define TEST {0x11,0x22,0x33,0x44,0x55,0x66}
#define ETH_P_MAPRELAY 0x6559
	unsigned char	h_source[ETH_ALEN] = GEL02;
	unsigned char	h_dest[ETH_ALEN] = GEL01;
	unsigned char test[ETH_ALEN] = TEST;


	if(skb == NULL) {
		mbr_dbg(debug_level, ANY, "SKB is null!\n");
		return NF_ACCEPT;
	}

	__skb_pull(skb, skb_network_offset(skb));
	//err = dev_hard_header(skb, skb->dev, ETH_P_IP, h_dest, h_source, skb->len);
	err = eth_header_dirty(skb, ETH_P_MAPRELAY/*ETH_P_IP*/, h_dest, h_source, skb->len);
	if (err < 0)
		mbr_dbg(debug_level, ANY, "eth_header_dirty ERROR!! %d\n", err );
	err = push_relay_mac(skb, test);
	if (err < 0)
		mbr_dbg(debug_level, ANY, "push_relay_mac ERROR!! %d\n", err );

//	mbr_dbg(debug_level, ANY, "OUTPUT: SAddress: %pI4, DAddress: %pI4\n", &ip_hdr(skb)->saddr, &ip_hdr(skb)->daddr);

	dev_queue_xmit(skb);

	return NF_STOLEN;
}
