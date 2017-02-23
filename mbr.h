#ifndef MBR_H
#define MBR_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>   //wake_up_process()
#include <linux/kthread.h> //kthread_create()��kthread_run()
#include <linux/err.h> //IS_ERR()��PTR_ERR()

#include <net/sock.h>
#include <linux/netlink.h>

#include <linux/debugfs.h>
#include <linux/types.h>

#include <linux/slab.h>

#include "pkt_output.h"
#include "pkt_input.h"
#include "debug.h"
#include "graph.h"
#include "geohash.h"
#include "mbr_route.h"
#include "neighbors.h"

//Shared memory page size.
#define SHARED_MEM_DEVNAME "mbr_neighbor_mem"
#define SHARED_MEM_SIZE	4*4096

#define NETLINK_USER  22
#define USER_MSG    (NETLINK_USER + 1)
#define USER_PORT   50


//struct mbr_common {
//
//	int debug_mask;
//};

struct mbr_status
{
	struct dentry *dir;
	u64 geohash_this;
};


typedef struct graph_deliver
{
#define DEL_NODE	1
#define NEW_NODE	2
#define NEW_EDGE	3
    short mode; 	//�������ͣ����ֶ�Ϊ1����ɾ��һ���ڵ㣬2�������һ���ڵ㣬3�������һ���ߣ�
    union
    {
        struct  //�������ӻ���ɾ��һ���ڵ㣬���ֶα�ʾ����ӻ��ߴ�ɾ���Ľڵ�id��
        {
            char vertex[25];
            u64 geohash;
        }vertex;	
        struct 	//��������һ���ߣ����ֶα�ʾ��ӵıߵ���ʼ����ֹ�ڵ��Լ��ñ����ڵ�·��id��
        {
            char from[25];
            char to[25];
            int road_id;
        }edge;
    }parameter;
}graph_deliver;


#endif /* MBR_H */
