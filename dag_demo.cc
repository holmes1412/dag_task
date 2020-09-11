#include <stdlib.h>
#include <stdio.h>
#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"
#include "dag_task.h"

struct my_ctx
{
	std::vector<int> data;
	int n;
};

static WFFacilities::WaitGroup wait_group(1);

void callback(DAGTask<int> task)
{
	wait_group.done();
}

int main()
{
	DAGTask<int> *dag = new DAGTask<int>([](DAGTask<int> *task) {
		printf("dag finish\n");
		wait_group.done();
	});

	WFTimerTask *a = WFTaskFactory::create_timer_task(1, [](WFTimerTask *task) {
		printf("task a finish\n");
	});

	WFTimerTask *b = WFTaskFactory::create_timer_task(1, [](WFTimerTask *task) {
		printf("task b finish\n");
	});

	dag->add_edge(a, b);
	dag->start();
	wait_group.wait();
	return 0;
}
