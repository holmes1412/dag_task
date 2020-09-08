#include <sys\stat.h>

#include "reduce_file_sort.h"

/*
bool cmp(const __VAL &a, const __VAL &b) 
{
    return a.val < b.val;
}
*/

void DAGReduceFactory::reduce_merge(const int *useless,
				   				   ReduceIterator<std::vector<DATA>> *data,
				   				   std::vector<DATA> *result,
								   CMP compare)
{
    auto *a = data->next();
    auto *b = data->next();
    std::merge(a->cbegin(), a->cend(), b->cbegin(), b->cend(), std::back_inserter(*result), compare);
}

void reduce_task_callback(WFReduceTask<int, std::vector<__VAL>> *task)
{
    auto *output = task->get_output();
    for (const auto& kv : *output)
    {
        //printf("%d : %d\n", kv.first, kv.second);
        for (int i = 0; i < kv.second.size(); i++)
            fprintf(stderr, "[%d] ", kv.second[i].val);
        fprintf(stderr, "\n");
    }
}

template<typename DATA, typename CMP>
void WFFileSortTask<DATA, CMP>::reader_callback(WFFileIOTask *task)
{
	SeriesWork *series = series_of(task);
	ReaderSorterCtx *ctx = (ReaderSorterCtx *)series->get_context();

	DATA data;
	WFSortTask<DATA> *sorter;

	while (parser_(ctx->buf, ctx->size, &data))
		ctx->data_array.emplace_back(std::move(data));

	sorter = WFAlgoTaskFactory::create_sort_task(std::string(ctx->id),
												 ctx->data_array.front(),
												 ctx->data_array.end(),
												 compare_,
												 sorter_callback_);
	free(ctx->buf);
	ctx->buf = NULL;
	series->push_back(sorter);
}

template<typename DATA, typename CMP>
void WFFileSortTask<DATA, CMP>::sorter_callback(WFSortTask<DATA> *task)
{
	SeriesWork *series = series_of(task);
	ReaderSorterCtx *ctx = (ReaderSorterCtx *)series->get_context();	
    merge_input_.push_back(std::make_pair(0, std::move(ctx->data_array)));
}

void all_memory_series_callback(const SeriesWork *series)
{
	ReaderSorterCtx *ctx = (ReaderSorterCtx *)series->get_context();
	ctx->counter->count();
	delete ctx;
}

template<typename DATA, typename CMP>
void WFFileSortTask<DATA, CMP>::counter_callback(WFCounterTask *task)
{
	close(read_fd_);
	auto *reducer = DAGReduceFactory::create_reduce_merge_task("reduce_merge",
														       std::move(merge_input_));
	series_of(task)->push_back(reducer);
	this->counter_ = NULL;
}

template<typename DATA, typename CMP>
void WFFileSortTask<DATA, CMP>::dispatch()
{
	SeriesWork *series;
	WFFileIOTask *reader;
	WFSorterTask *sorter;
	ReaderSorterCtx *ctx;
	int pack_size, read_size;

	// 1. check file size
	struct stat file_info;
	if (stat(file_name_, &file_info) < 0
		|| (read_fd_ = open(file_name_, O_RDONLY)) == -1)
	{
		this->state = WFT_STATE_SYS_ERROR;
		this->error = errno;
		this->subtask_done;
		return;
	}
	file_size_ = file_info.st_size;

	// 2. calc for stradegy-1 (all memory, no middle tmp file will be created)
	// 	  or stradegy-2 (sort n round and output n tmp file, then n-merge for n tmp file)

	if (file_size_ * 2 < mem_limit_)
	{
		pack_size = (file_size_ + parallelism_ - 1) / parallelism_;
		read_size = pack_size + sizeof(DATA) - 1;

		counter_ = WFTaskFactory::create_counter_task(parallelism_, counter_callback);
		series_of(this)->push_front(counter_);

		// 3. stradegy-1: make p series
		for (int i = 0; i < parallelism_; i++)
		{
			reader = WFTaskFactory::create_pread_task(read_fd_, buf, read_size, i * (pack_size),
													  reader_callback_);

			ctx = new ReaderSorterCtx();
			ctx->buf = malloc(read_size);
			ctx->id = i;
			ctx->size = reader_size;
			ctx->counter = counter_;

			series = Workflow::create_series_work(reader, all_memory_series_callback);
			series->set_context(ctx);
			series->start(); // can be more strict
		}
	}
	else
	{
		// 4. stradegy-2 TODO
	}

	this->subtask_done();
}

template<typename DATA, typename CMP>
SubTask *WFFileSortTask<DATA, CMP>::done()
{
	SeriesWork *series = series_of(this);

	if (!counter_)
	{   
		if (callback_)
			callback_(this);

		delete this;
	}   

	return series->pop();
}
