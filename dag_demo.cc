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

void prepare_a(SubTask *src, my_ctx *ctx)
{
	ctx->a = 1;
}

void prepare_b(SubTask *src, my_ctx *ctx)
{
	ctx->b = ctx->a + 1;
}

void prepare_c(SubTask *src, my_ctx *ctx)
{
	ctx->c = ctx->a + 2;
}

void prepare_d(SubTask *src, my_ctx *ctx)
{
	ctx->d = ctx->b + ctx->c;
}

int main()
{
	DAGTask<my_ctx> *dag = new DAGTask<my_ctx>([](DAGTask<my_ctx> *task) {
		printf("==> dag finish. d = %d\n", task->get_ctx()->d);
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

	dag->add_preparation(a, prepare_a);
	dag->add_preparation(b, prepare_b);
	dag->add_preparation(c, prepare_c);
	dag->add_preparation(d, prepare_d);

	struct my_ctx ctx;
	dag->set_ctx(&ctx);

	dag->start();
	wait_group.wait();

	return 0;
}
