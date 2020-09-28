# dag_task

由于workflow的series和parallel基本可以解决串行和并行的任务流图问题，对于多入边依赖的任务，workflow提供了[WFContainerTask](https://github.com/sogou/workflow/blob/master/src/factory/WFContainerTask.h)。所以dag_task是利用了``WFContainerTask``，为workflow的任务而开发的单进程DAG任务，目标是帮助用户把一些通过workflow创建的任务按照业务逻辑，组织构建成一个DAG业务流图并按任务依赖关系来执行。

### 代码
具体实现可以查看[dag_task.h](dag_task.h)，用例可以查看[dag_demo.cc](dag_demo.cc)。

### 编译
首先workflow对于WFContainerTask.h没有挪到外部用，我们需要先到workflow的目录下打个patch，并重新编译：
~~~shell
cd ${WORKFLOW_DIR}
patch -p 1 < ${DAG_TASK_DIR}/patch.1
make
~~~

然后我们回到dag_task的目录，编译时需要指定workflow的依赖路径，比如：
~~~shell
cd ${DAG_TASK_DIR}
make Workflow_DIR=/search/ted/1412/workflow_1412
~~~

### 示例
我们以``dag_demo.cc``为例：

<img src="/images/dag_task.png" width = "500" height = "400" alt="dag_task" align=center />

这个例子是构建一个a、b、c、d的DAG。其中四个节点可以是任意一种任务，我们这里为了方便，用了``WFTimerTask``。

因为DAGTask需要承载一些数据，所以整个DAG的数据是它的模版，也就是例子中的``my_ctx``。

我们需要做的事情是：

1. 准备好各个节点，比如a、b、c、d，节点可以从Workflow现有的协议创建，或者是自己的算法task；
~~~cpp
    WFTimerTask *a = WFTaskFactory::create_timer_task(1000000, task_a_callback);
    ...
~~~

2. 创建一个DAGTask，可以传入回调函数，用法与workflow其他task一致；
~~~cpp
    DAGTask<my_ctx> *dag = new DAGTask<my_ctx>(dag_task_callback);
~~~

3. 通过DAGTask的接口``bool add_edge(SubTask *src, SubTask *dst)``把a、b、c、d逻辑依赖加上，也就是DAG的边。
~~~cpp
    dag->add_edge(a, b);
    dag->add_edge(a, c);
    ...
~~~

4. 我们的dag总是涉及数据，所以我们提供了preparation，准备函数要做的事情是：这个节点执行前，需要从全局ctx拿什么数据下来；每个任务可以通过以下接口传入准备函数：
~~~cpp
    using preparation_t = std::function<void (SubTask *src, T *ctx)>;
    void add_preparation(SubTask *src, preparation_t preparation);
~~~
在demo中把函数绑定到具体的task上：
~~~cpp
    dag->add_preparation(a, prepare_a);
~~~

5. 把全局想要操作的ctx通过``void set_ctx(T *ctx)``接口挂上DAGTask:
~~~cpp
    struct my_ctx ctx; // 需要使用者保证生命周期与dag_task一致，如果是堆变量，可以在dag_task_callback中delete
    dag->set_ctx(&ctx);
~~~

6. 构建好DAG后，执行``start()``，其中的各个节点就可以按照依赖关系被调起，并且通过``preparation``进行节点间的数据搬运。

7. 由于是异步任务，记得等回来之后再退出进程，所以我们的例子里用了一个``WFFacilities::WaitGroup``来卡住，并且在``dag_task_callback``中``done()``以示结束。


最后。

补充说明，例子里是计算：

    a = 1; b = a + 1; c = a + 2; d = b + c;

这些计算步骤其实放到哪里都可以实现，这里只是为了体现如何实现dag节点间数据传递的做法，放到了preparation阶段做。实际上，只要保证可以看到ctx，它可以在计算节点本身、或者节点的callback里做。
