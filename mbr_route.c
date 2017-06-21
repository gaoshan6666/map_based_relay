#include "mbr_route.h"
#include "mbr.h"
#include "common.h"
#include "utils.h"
#include "neighbors.h"
#include <string.h>

LIST_HEAD(mbrtable);

int bitcmp(uint64_t a, uint64_t b, int step)
{
	return a==b;
}
/**
 * Search mbr table for the GeoHash of next-hop.
 * input: dest node's geohash
 * return: valid next-hop's geohash if exist.
 */
uint64_t mbrtable_get_nexthop_geohash(uint64_t dstGeoHash)
{
	relay_table_list *entry;
	list_for_each_entry(entry, &mbrtable, ptr){
		if(bitcmp(entry->geoHash_dst, dstGeoHash, GEOHASH_STEP_BIT) && entry->isvalid)
			return entry->geoHash_nexthop;
	}
	return 0;
}

uint64_t get_geohash_this(void)
{
	if (global_mbr_status.geohash_this == 0) {
		mbr_dbg(debug_level, ANY,"get_geohash_this get zero!!\n");
	}

	return global_mbr_status.geohash_this;
}

/**
 * �����ڵ��geohash�����仯ʱ��Ҫ��mbr�м̱���и���
 */
int update_mbrtable_outrange(uint64_t updated_geohash)
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
int update_mbrtable(uint64_t this_geohash, uint64_t dst_geohash, uint64_t nexthop_geohash)
{
	relay_table_list *entry;

	list_for_each_entry(entry, &mbrtable, ptr){
		if(bitcmp(entry->geoHash_dst, dst_geohash, GEOHASH_STEP_BIT)) {
			entry->geoHash_nexthop = nexthop_geohash;
			entry->isvalid = 1;
			goto out;
		}
	}
	entry = (relay_table_list *)mbr_malloc(sizeof(relay_table_list));
	entry->geoHash_dst = dst_geohash;
	entry->geoHash_nexthop = nexthop_geohash;
	entry->geoHash_this = this_geohash;
	entry->isvalid = 1;
	list_add(&entry->ptr, &mbrtable);
out:
	return 0;
}

#ifdef LINUX_KERNEL
/**
 * print mbrtable
 */
void print_mbrtable(struct seq_file *file)
{
	relay_table_list *entry;
	seq_puts(file, "Mbrtable:\n");
	seq_puts(file, "geoHash_this, geoHash_dst, geoHash_nexthop\n");
	list_for_each_entry(entry, &mbrtable, ptr){
		seq_printf(file, "%lld     %lld      %lld\n", entry->geoHash_this, entry->geoHash_dst, entry->geoHash_nexthop);
	}
	return;
}

int mbr_forward(uint8_t *dst_mac, uint8_t *relay_mac, struct sk_buff *skb, Graph *g)
{
	/**
	 * ��skb��nexthop�����ھӱ�õ��õ��Geohash
	 */
	int i;
	struct dst_entry *dst = skb_dst(skb);
	struct rtable *rt = (struct rtable *)dst;
	uint32_t nexthop;//�洢skb�а�����·����һ����ip
	uint64_t nexthop_geohash = 0;
	uint64_t this_geohash = 0;
	uint64_t dst_geohash = 0;
	Vertex *intersection;
	Vertex *this_vertex;
	Vertex *dst_vertex;
	//GeoHashBits	geohashbit_tmp;
	neighbor_table* neighbor_entry;
	GeoHashSetCoordinate geohashset;
	int ret;

	/**
	 * ���ھӱ����ҵ���һ��ip��ע���ʱ�ġ���һ��ip����������·��ָ������һ��ip���������м̽ڵ��ip����Ӧ��geohash
	 */
	nexthop = (__force uint32_t) rt_nexthop(rt, ip_hdr(skb)->daddr);
	ret = neighbor_getnode_fromip(&neighbor_entry, nexthop);
	if(ret == 0) {
		dst_geohash = neighbor_entry->geoHash;
		memcpy(dst_mac, neighbor_entry->mac, 6);
	} else {
		mbr_dbg(debug_level, ANY, "mbr_forward: nexthop does not exist in the neighbors!\n");
		return -1;
	}

#ifdef CONFIG_MBR_TABLE
	/**
	 * ���м̱�õ��м�geohash���õ��Ľ���Ѿ��ǡ��м̽ڵ㡱��geohash
	 */
	nexthop_geohash = mbrtable_get_nexthop_geohash(dst_geohash);
	if(nexthop_geohash == 0){ //miss
		/**
		 * �����м����򣬲������м̱�
		 */
		this_geohash = get_geohash_this();
		this_vertex = find_Vertex_by_VehiclePosition(g, this_geohash);
		dst_vertex = find_Vertex_by_VehiclePosition(g, dst_geohash);
		intersection = cross_vertex(this_vertex, dst_vertex);
		if(intersection != NULL)
			nexthop_geohash = intersection->geoHash;
		else
			return -1;
		//dst_geohash = neighbor_getgeohash_fromip(ip_hdr(skb)->daddr);
		update_mbrtable(this_geohash, dst_geohash, nexthop_geohash);
	}
#else
	this_geohash = get_geohash_this();
	this_vertex = find_Vertex_by_VehiclePosition(g, this_geohash);
	dst_vertex = find_Vertex_by_VehiclePosition(g, dst_geohash);
	intersection = cross_vertex(this_vertex, dst_vertex);
	if(intersection != NULL)
		nexthop_geohash = intersection->geoHash;
	else
		return -1;
#endif
	/**
	 * ��Geohash����·��ת��:
	 * 1. ͨ��Geohash����ܱ��ھӿ鹲ͬ���·��
	 * 2. ��ȡ�鼯���еĽڵ���Ϣ
	 * 3. �������һ���ڵ���Ϊ�м̽ڵ�
	 */
	setIntersectionSize(&geohashset, this_vertex, dst_vertex);

	//get neighbors of center geohash block
	geohash_get_neighbors_in_set(&geohashset, nexthop_geohash, GEOHASH_STEP_BIT);

	//ret = neighbor_getnode_fromset_random(&neighbor_entry, &geohashset);

	ret = neighbor_getnode_fromset_best(&neighbor_entry, &geohashset);
	if(ret == 0)
		return -1; //unmatched!

	memcpy(relay_mac, neighbor_entry->mac, 6);

	return 0;
}

#else

int mbr_forward(uint8_t * to, uint8_t * relay_mac, Graph *g)
{
	/**
	 * �����ھӱ�õ��õ��Geohash
	 */
	//int i;

	uint64_t nexthop_geohash = 0;
	uint64_t this_geohash = 0;
	uint64_t dst_geohash = 0;
	Vertex *intersection;
	Vertex *this_vertex;
	Vertex *dst_vertex;
	//GeoHashBits	geohashbit_tmp;
	neighbor_table* neighbor_entry;
	GeoHashSetCoordinate geohashset;
	int ret;


	dst_geohash = neighbor_getgeohash_frommac(to);
	if(dst_geohash == 0) {
		mbr_dbg(debug_level, ANY, "mbr_forward: nexthop does not exist in the neighbors!\n");
		return -1;
	}

#ifdef CONFIG_MBR_TABLE
	/**
	 * ���м̱�õ��м�geohash���õ��Ľ���Ѿ��ǡ��м̽ڵ㡱��geohash
	 */
	nexthop_geohash = mbrtable_get_nexthop_geohash(dst_geohash);
	if(nexthop_geohash == 0){ //miss
		/**
		 * �����м����򣬲������м̱�
		 */
		this_geohash = get_geohash_this();
		this_vertex = find_Vertex_by_VehiclePosition(g, this_geohash);
		dst_vertex = find_Vertex_by_VehiclePosition(g, dst_geohash);
		intersection = cross_vertex(this_vertex, dst_vertex);
		if(intersection != NULL)
			nexthop_geohash = intersection->geoHash;
		else
			return -1;
		//dst_geohash = neighbor_getgeohash_fromip(ip_hdr(skb)->daddr);
		update_mbrtable(this_geohash, dst_geohash, nexthop_geohash);
	}
#else
	this_geohash = get_geohash_this();
	this_vertex = find_Vertex_by_VehiclePosition(g, this_geohash);
	dst_vertex = find_Vertex_by_VehiclePosition(g, dst_geohash);
	intersection = cross_vertex(this_vertex, dst_vertex);
	if(intersection != NULL)
		nexthop_geohash = intersection->geoHash;
	else
		return -1;
#endif
	/**
	 * ��Geohash����·��ת��:
	 * 1. ͨ��Geohash����ܱ��ھӿ鹲ͬ���·��
	 * 2. ��ȡ�鼯���еĽڵ���Ϣ
	 * 3. �������һ���ڵ���Ϊ�м̽ڵ�
	 */
	setIntersectionSize(&geohashset, this_vertex, dst_vertex);

	//get neighbors of center geohash block
	geohash_get_neighbors_in_set(&geohashset, nexthop_geohash, GEOHASH_STEP_BIT);

	//ret = neighbor_getnode_fromset_random(&neighbor_entry, &geohashset);

	ret = neighbor_getnode_fromset_best(&neighbor_entry, &geohashset);
	if(ret == 0)
		return -1; //unmatched!

	memcpy(relay_mac, neighbor_entry->mac, 6);

	return 0;
}


#endif /* LINUX_KERNEL */
