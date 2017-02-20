#include "mbr_route.h"
#include <net/route.h>

LIST_HEAD(mbrtable);

extern struct mbr_status global_mbr_status;
extern int debug_level;

int bitcmp(u64 a, u64 b, int step)
{
	return a==b;
}
/**
 * Search mbr table for the GeoHash of next-hop.
 * input: dest node's geohash
 * return: valid next-hop's geohash if exist.
 */
u64 mbrtable_get_nexthop_geohash(u64 dstGeoHash)
{
	relay_table_list *entry;
	list_for_each_entry(entry, &mbrtable, ptr){
		if(bitcmp(entry->geoHash_dst, dstGeoHash, GEOHASH_STEP_BIT) && entry->isvalid)
			return entry->geoHash_nexthop;
	}
	return 0;
}

u64 get_geohash_this(void)
{
	if (global_mbr_status.geohash_this == 0) {
		mbr_dbg(debug_level, ANY,"get_geohash_this get zero!!\n");
	}

	return global_mbr_status.geohash_this;
}

/**
 * �����ڵ��geohash�����仯ʱ��Ҫ��mbr�м̱���и���
 */
int update_mbrtable_outrange(u64 updated_geohash)
{
	relay_table_list *entry;
	list_for_each_entry(entry, &mbrtable, ptr){
		if(bitcmp(entry->geoHash_this, updated_geohash, GEOHASH_STEP_BIT)) {

			entry->isvalid = 0;

		}
	}
	return 0;
}

/**
 * ����Ҫ������dst_geohash�ļ�¼���еĻ��ͽ��и��£�û�еĻ���Ҫ����һ����¼
 */
int update_mbrtable(u64 this_geohash, u64 dst_geohash, u64 nexthop_geohash)
{
	relay_table_list *entry;

	list_for_each_entry(entry, &mbrtable, ptr){
		if(bitcmp(entry->geoHash_dst, dst_geohash, GEOHASH_STEP_BIT)) {
			entry->geoHash_nexthop = nexthop_geohash;
			entry->isvalid = 1;
			goto out;
		}
	}
	entry = (relay_table_list *)kmalloc(sizeof(relay_table_list), GFP_KERNEL);
	entry->geoHash_dst = dst_geohash;
	entry->geoHash_nexthop = nexthop_geohash;
	entry->geoHash_this = this_geohash;
	entry->isvalid = 1;
	list_add(entry, &mbrtable);
out:
	return 0;
}

int mbr_forward(u8 *dst_mac, u8 *relay_mac, struct sk_buff *skb, Graph *g)
{
	/**
	 * ��skb��nexthop�����ھӱ�õ��õ��Geohash
	 */
	struct dst_entry *dst = skb_dst(skb);
	struct rtable *rt = (struct rtable *)dst;
	u32 nexthop; //�洢skb�а�����·����һ����ip
	u64 nexthop_geohash = 0;
	u64 this_geohash = 0;
	u64 dst_geohash = 0;
	Vertex *intersection;
	Vertex *this_vertex;
	Vertex *nexthop_vertex;
	GeoHashBits	geohashbit_tmp;
	neighbor_table* neighbor_entry;
	u64 geohashset[9];
	int ret;

	/**
	 * ���ھӱ����ҵ���һ��ip��ע���ʱ�ġ���һ��ip����������·��ָ������һ��ip���������м̽ڵ��ip����Ӧ��geohash
	 */
	nexthop = (__force u32) rt_nexthop(rt, ip_hdr(skb)->daddr);
	ret = neighbor_getnode_fromip(neighbor_entry, nexthop);
	if(ret == 0) {
		nexthop_geohash = neighbor_entry->geoHash;
		dst_mac = neighbor_entry->mac;
	} else {
		mbr_dbg(debug_level, ANY, "mbr_forward: nexthop does not exist in the neighbors!\n");
		return -1;
	}
	/**
	 * ���м̱�õ��м�geohash���õ��Ľ���Ѿ��ǡ��м̽ڵ㡱��geohash
	 */
	nexthop_geohash = mbrtable_get_nexthop_geohash(nexthop_geohash);

	if(nexthop_geohash == 0){ //miss
		/**
		 * �����м����򣬲������м̱�
		 */
		this_geohash = get_geohash_this();
		this_vertex = find_Vertex_by_VehiclePosition(g, this_geohash);
		nexthop_vertex = find_Vertex_by_VehiclePosition(g, nexthop_geohash);
		intersection = cross_vertex(this_vertex, nexthop_vertex);
		nexthop_geohash = intersection->geoHash;
		dst_geohash = neighbor_getgeohash_fromip(ip_hdr(skb)->daddr);
		update_mbrtable(this_geohash, dst_geohash, nexthop_geohash);
	}
	/**
	 * ��Geohash����·��ת��:
	 * 1. ͨ��Geohash����ܱ��ھӿ鹲ͬ���·��
	 * 2. ��ȡ�鼯���еĽڵ���Ϣ
	 * 3. �������һ���ڵ���Ϊ�м̽ڵ�
	 */
	geohashbit_tmp.step = GEOHASH_STEP_BIT;
	geohashbit_tmp.bits = nexthop_geohash;
	geohash_get_neighbors_in_set(geohashbit_tmp, geohashset);
	ret = neighbor_getnode_fromset_random(neighbor_entry, geohashset, 9);
	if(ret == 0)
		return -1; //unmatched!
	relay_mac = neighbor_entry->mac;

	return 0;
}

