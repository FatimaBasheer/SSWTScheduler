/*
 * elevator noop
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct sswt_data {
	struct list_head queue;//list of the pending requests
	sector_t head_pos; //sector number of the head
};

//request queue maintains the pending block i/o requests
static void noop_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
//deletes entry from list 
	list_del_init(&next->queuelist);
}

//gets the distance between the head of the platter and the sector of the next request(always positive)
static unsigned long get_distance(struct request *rq, struct sswt_data *sd)
{
        sector_t req_sector = blk_rq_pos(rq);
        if(sd->head_pos > req_sector)
                return sd->head_pos - req_sector;
        else
                return req_sector - sd->head_pos;
}
//dispatches a request to disk based on the algorithm
static int noop_dispatch(struct request_queue *q, int force)
{
	unsigned long tv=5;					
	struct sswt_data *sd = q->elevator->elevator_data;

	if (!list_empty(&sd->queue)) {
		
		struct request *rq,*min_req;
//get the struct for the entry sd->queue.next

		rq = list_entry(sd->queue.next, struct request, queuelist);
		min_req = rq;
		
		struct list_head *pos;
		struct request *cur_req;
//iterate through a list
		unsigned long max_tv=tv-1;			//written by us
		list_for_each(pos, &sd->queue)
       		 {	
		        cur_req = list_entry(pos, struct request, queuelist);
						
			if(cur_req->wait_count > max_tv)
			{
				max_tv=cur_req->wait_count;
				min_req=cur_req;					
			}
		        if( (curr_req->wait_count==max_tv || max_tv < tv) && (get_distance(min_req, sd) > get_distance(cur_req, sd)) )
		        {
						min_req = cur_req;
		        }
			cur_req->wait_count++;
       		 }
		
		rq = min_req;
		list_del_init(&rq->queuelist);
		elv_dispatch_add_tail(q, rq);
		sd->head_pos = blk_rq_pos(rq) + blk_rq_sectors(rq);
		printk("sswt dispatch %llu\n",blk_rq_pos(rq));	
//adding rq to the back of the dispatch queue
		return 1;
	}
	return 0;
}
//adds a disk request to the queue of pending requests
static void noop_add_request(struct request_queue *q, struct request *rq)
{
    struct sswt_data *nd = q->elevator->elevator_data;
	list_add_tail(&rq->queuelist, &nd->queue);
	rq->wait_count=0;
	printk("sswt add %llu\n",blk_rq_pos(rq));
}

static struct request *
noop_former_request(struct request_queue *q, struct request *rq)
{
	struct sswt_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
noop_latter_request(struct request_queue *q, struct request *rq)
{
	struct sswt_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}
//performs necessary initialization activities
static int noop_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sswt_data *nd;
	struct elevator_queue *eq;
//allocates zeroed memory from a particular memory node
//zero mem pages that are zeroed out and are ready to use
//returns elevator queue
	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;
//Out of Memory

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
//allocate mem from a specific node
	if (!nd) {
//decrementing the ref count
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;
//initialization of list head of the queue from nd
	INIT_LIST_HEAD(&nd->queue);
	nd->head_pos = 0;
//to lock the queue 
	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void noop_exit_queue(struct elevator_queue *e)
{
	struct sswt_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}

static struct elevator_type elevator_noop = {
	.ops = {
		.elevator_merge_req_fn		= noop_merged_requests,
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
		.elevator_former_req_fn		= noop_former_request,
		.elevator_latter_req_fn		= noop_latter_request,
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
	},
	.elevator_name = "sswt",
	.elevator_owner = THIS_MODULE,
};

static int __init noop_init(void)
{
	return elv_register(&elevator_noop);
}

static void __exit noop_exit(void)
{
	elv_unregister(&elevator_noop);
}

module_init(noop_init);
module_exit(noop_exit);


MODULE_AUTHOR("ARGH");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");


