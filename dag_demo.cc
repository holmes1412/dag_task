#include <stdlib.h>
#include <stdio.h>
#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"
#include "dag_task.h"

struct my_ctx
{
	int a;
	int b;
	int c;
	int d;
};

static WFFacilities::WaitGroup wait_group(1);

void callback(DAGTask<int> task)
{
	wait_group.done();
}

int main()
{
	DAGTask<my_ctx> *dag = new DAGTask<my_ctx>([](DAGTask<my_ctx> *task) {
		printf("==> dag finish\n");
		wait_group.done();
	});

	WFTimerTask *a = WFTaskFactory::create_timer_task(1000000, [](WFTimerTask *task) {
		printf("==> task a finish\n");
	});

	WFTimerTask *b = WFTaskFactory::create_timer_task(1000000, [](WFTimerTask *task) {
		printf("==> task b finish\n");
	});

	WFTimerTask *c = WFTaskFactory::create_timer_task(2000000, [](WFTimerTask *task) {
		printf("==> task c finish\n");
	});

	WFTimerTask *d = WFTaskFactory::create_timer_task(1000000, [](WFTimerTask *task) {
		printf("==> task d finish\n");
	});

	dag->add_edge(a, b);
	dag->add_edge(a, c);
	dag->add_edge(b, d);
	dag->add_edge(c, d);

	struct my_ctx ctx;
	dag->set_ctx(&ctx);

	dag->start();
	wait_group.wait();

	return 0;
}
