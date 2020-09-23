#ifndef __WFDAGTASK__
#define __WFDAGTASK__

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <vector>
#include <unistd.h>
#include <signal.h>

#include "workflow/WFTaskFactory.h"
#include "workflow/WFContainerTask.h"

#include <queue>
#include <vector>
#include <unordered_map>

#define WFT_DAG_HAS_CYCLES 1979

// DirectedAcyclicGraphTask
template<typename T>
class DAGTask : public WFGenericTask
{
using preparation_t = std::function<void (SubTask *src, T *ctx)>;

public:
	bool add_edge(SubTask *src, SubTask *dst);
	void add_preparation(SubTask *src, preparation_t preparation);

	void set_ctx(T *ctx) { this->ctx = ctx; }

	DAGTask(std::function<void (DAGTask *)>&& callback) :
		dag_cb(std::move(callback)),
		end_container(NULL)
	{
	}

private:
    virtual void dispatch();
    virtual SubTask *done();

	void series_callback(const SeriesWork *series);
	void container_callback(WFContainerTask<T> *task);

private:
	T *ctx;

	std::unordered_map<SubTask *, int> in_count;
	std::unordered_map<SubTask *, std::vector<SubTask *>> edge_map;
	std::unordered_map<SubTask *, preparation_t> preparation_map;
	std::unordered_map<WFContainerTask<T> *, SubTask *> container_next;
 
	std::function<void (DAGTask *)> dag_cb;
	WFContainerTask<T> *end_container;
	std::vector<WFContainerTask<T> *> end_vector;
};

template<typename T>
bool DAGTask<T>::add_edge(SubTask *src, SubTask *dst)
{
	printf("add edge from task %p to task %p\n", src, dst);
	if (series_of(src) || series_of(dst))
	{
		printf("add edge failed\n");
		return false;
	}

	std::unordered_map<SubTask *, int>::iterator in_iter;
	std::unordered_map<SubTask *, std::vector<SubTask *>>::iterator edge_iter;

	// 1. update everything use src as index
	in_iter = this->in_count.find(src);
	if (in_iter == this->in_count.end())
		this->in_count.insert(std::make_pair(src, 0));
	
	edge_iter = this->edge_map.find(src);
	if (edge_iter == this->edge_map.end())
	{
		std::vector<SubTask *> dst_nodes;
		dst_nodes.push_back(dst);
		this->edge_map.insert(std::make_pair(src, std::move(dst_nodes)));
	}
	else
	{
		edge_iter->second.push_back(dst);
	}

	// 2. update everything use dst as index
	in_iter = this->in_count.find(dst);
	if (in_iter == this->in_count.end())
		this->in_count.insert(std::make_pair(dst, 1));
	else
		in_iter->second++;

	return true;
}

template<typename T>
void DAGTask<T>::add_preparation(SubTask *src, preparation_t preparation)
{
	const auto p_iter = this->preparation_map.find(src);
	if (p_iter == this->preparation_map.c_end())
		this->preparation_map.insert(std::make_pair(src, std::move(preparation)));
	else
		p_iter->second = std::move(preparation);
}

template<typename T>
void DAGTask<T>::series_callback(const SeriesWork *series)
{
	std::vector<WFContainerTask<T> *> *dst_containers =
			(std::vector<WFContainerTask<T> *> *)series->get_context();
	for (int i = 0; i < dst_containers->size(); i++)
	{
		printf("sereis-%p callback. will push to next container:%p\n",
				series, (*dst_containers)[i]);
		(*dst_containers)[i]->push_empty();
	}
}

template<typename T>
void DAGTask<T>::container_callback(WFContainerTask<T> *task)
{
	// 1. find next_task
	const auto con_iter = this->container_next.find(task);

	if (con_iter != this->container_next.end())	
	{
		printf("container %p find next task %p and start\n",
				task, con_iter->second);
		// 2. find next_task->prepare
		const auto p_iter = this->preparation_map.find(task);
		if (p_iter != this->preparation_map.end())
			p_iter->second(con_iter->second, this->ctx);

		// 3. start
		series_of(con_iter->second)->start();
		//delete task;
	}
	else // this is the end container
	{
		printf("container %p reach end\n", this->end_container);

		this->end_container = NULL;
		if (this->dag_cb)
			this->dag_cb(this);

		delete this;
	}
}

template<typename T>
void DAGTask<T>::dispatch()
{
	if (this->state != WFT_STATE_UNDEFINED)
	{
		this->subtask_done();
		return;
	}

	std::unordered_map<SubTask *, WFContainerTask<T> *> container_map;
	std::unordered_map<SubTask *, bool> check_map;
	std::queue<SubTask *> queue;

	printf("\n[CHECK EDGE]\ncheck in_count. size = %zu\n", this->in_count.size());
	for (const auto &kv : this->in_count)
	{
		// 1. prepare for queue
		if (kv.second == 0)
		{
			check_map.insert(std::make_pair(kv.first, true));
			queue.push(kv.first);
		}
		else
		{
			check_map.insert(std::make_pair(kv.first, false));
			// 2. make container
			WFContainerTask<T> *c = new WFContainerTask<T>(
										kv.second,
										std::move(std::bind(&DAGTask::container_callback,
									   				   		this, std::placeholders::_1))
										);
			Workflow::start_series_work(c, nullptr);
			this->container_next.insert(std::make_pair(c, kv.first));
			container_map.insert(std::make_pair(kv.first, c));
			printf("task %p in_count=%d container of this is: %p\n", kv.first, kv.second, c);
		}
	}

	if (queue.size() == 0)
	{
		this->state = WFT_STATE_SYS_ERROR;
		this->error = WFT_DAG_HAS_CYCLES;
		this->subtask_done();
		return;
	}

	std::unordered_map<SubTask *, std::vector<SubTask *>>::iterator edge_iter;
	SubTask *src;
	SubTask *dst;
	WFContainerTask<T> *dst_container;
	SeriesWork *series;
	std::vector<SubTask *> out_count;

	printf("\n[CHECK QUEUE]\n");
	// 3. deal with each src node
	while (queue.size())
	{
		src = queue.front();
		printf("queue.size=%zu get task %p\n", queue.size(), src);
		queue.pop();

		if (series_of(src))
		{
			printf("ERROR. task %p already in series\n");
			this->subtask_done();
			return;
		}

		series = Workflow::create_series_work(src,
											  std::bind(&DAGTask::series_callback,
											  this, std::placeholders::_1));

		// deal with each edge
		edge_iter = this->edge_map.find(src);
		if (edge_iter != this->edge_map.end())
		{
			// each dst for this src
			for (int i = 0; i < edge_iter->second.size(); i++)
			{
				dst = edge_iter->second[i];
				if (check_map[dst] == true)
				{
					// TODO: check cycles
				}
				else
				{
					queue.push(dst);
					check_map[dst] = true;
				}

				// change dst into dst->container
				dst = edge_iter->second[i];
				dst_container = container_map[dst];
				edge_iter->second[i] = dst_container;
			}
			
			// change the callback and makr all the dst_containers
			series->set_context(&edge_iter->second);
		}
		else // I don`t have any next, set as end
			out_count.push_back(src);
	}

	// 4. make my end_container
	printf("\n[ADD END CONTAINER]\n");
	this->end_container = new WFContainerTask<T>(out_count.size(),
												 std::move(std::bind(&DAGTask::container_callback,
									   			 this, std::placeholders::_1)));
	Workflow::start_series_work(this->end_container, nullptr);
	this->end_vector.push_back(this->end_container);
	for (int i = 0; i < out_count.size(); i++)
	{
		printf("make task %p link to end_container %p\n",
				out_count[i], this->end_container);
		series_of(out_count[i])->set_context(&this->end_vector);
	}
	printf("\n[DISPATCH ALL]\n");

	// 5. start nodes
	for (const auto &kv : this->in_count)
	{
		if (kv.second == 0)
			series_of(kv.first)->start();
	}

	this->subtask_done();	
}

template<typename T>
SubTask *DAGTask<T>::done()
{
    SeriesWork *series = series_of(this);

    if (!this->end_container)
    {   
        if (this->dag_cb)
            this->dag_cb(this);

        delete this;
    }   

    return series->pop();
}

#endif
