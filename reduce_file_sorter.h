#ifndef __WFREDUCEFILESORT__
#define __WFREDUCEFILESORT__

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <vector>
#include <unistd.h>
#include <signal.h>

#include "workflow/WFTaskFactory.h"
#include "workflow/MapReduce.h"

using namespace algorithm;

#define REDUCE_SORT_X_FACTOR 2

using parser_t = std::function<bool (void *buf, size_t buflen, DATA& data)>;

using file_sort_callback_t = std::function<void (WFFileSortTask *)>;

using reader_callback_t = std::function <void (WFFileIOTask *task)>;

using sorter_callback_t = std::function <void (WFSortTask<DATA> *task)>;

template<typename DATA>
class ReaderSorterCtx
{
	void *buf;
	int size;
	int id;
	std::vector<DATA> data_array;
	WFCounterTask *counter;
};

template<typename DATA>
class WFReduceMergeTask
{
public:
	WFReduceMergeTask()
	{
	}
};

class WFReduceFileMergeTask
{
public:
	WFReduceFileMergeTask(const std::vector<std::string>& file_names,
							...
						)
};

template<typename DATA, typename CMP>
class WFFileSortTask : public WFGenericTask
{
public:
	WFFileSortTask(const std::string& file_name,
				   parser_t parse,
				   size_t men_limit,
				   size_t parallelism,
				   CMP compare,
				   file_sort_callback_t cb) :
			file_name_(file_name),
			parse_(std::move(parse)),
			compare_(std::move(compare)),
			mem_limit_(men_limit),
			parallelism_(parallelism),
			callback_(std::move(cb))
	{
		counter_ = NULL;
		has_next_ = false;
		x_factor_ = REDUCE_SORT_X_FACTOR;
		reader_callback_ = std::bind(&this->reader_callback, &this, std::placeholders::_1);
		sorter_callback_ = std::bind(&this->sorter_callback, &this, std::placeholders::_1);
	}

private:
	virtual void dispatch();
	virtual SubTask *done();

	void reader_callback(WFFileIOTask *task);
	void sorter_callback(WFSortTask<DATA> *task);

private:
	std::string file_name_;
	parser_t parse_;
	CMP compare_;
	file_sort_callback_t callback_;
	reader_callback_t reader_callback_;
	sorter_callback_t sorter_callback_;

	int x_factor_;
	int mem_limit_;
	int file_size_;
	WFCounterTask *counter_; // for n-merge if necessary
	bool has_next_task_;
	int read_fd_;
	int pack_size_; // size for each reader

	// use KEY=int, because for sort, KEY is useless in reduce
	ReduceInput<int, std::vector<DATA>> merge_input_;
};

template<typename DATA, class CMP>
class DAGReduceFactory
{

// typedef KEY std::vector<DATA>;
// typedef VAL std::vector<DATA>;
// I may have parameter x, n = parallelism * x; x = 2 so far

WFFileSortTask<DATA, CMP> *create_sort_task(const std::string& file_name,
										   parser_t parse,
										   size_t men_limit,
										   size_t parallelism,
										   CMP compare,
										   file_sort_callback_t cb) :
{
	auto *task = new WFFileSortTask(file_name, parse, men_limit, parallelism, compare, cb);
	return task;
}

WFReduceTask<int, std::vector<DATA>> *create_reduce_merge_task(const std::string& task_name,
															   ReduceInput<int, std::vector<DATA>>&& input)
{
	auto *task = WFAlgoTaskFactory::create_reduce_task<int, std::vector<DATA>>(task_name,
																			   std::move(input),
																			   reduce_merger,
																			   reduce_task_callback);
	return task;
}

WFReduceFileMergeTask<Data> *create_reduce_merge_task(std::vector<std::string>& file_names,
													  ...)
{
	//auto *task = new WFReduceFileMergeTask(file_names, );
}



}; // end DAGReduceFactory

#endif
