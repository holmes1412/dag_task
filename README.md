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

然后我们回到da_task的目录，编译时需要指定workflow的依赖路径，比如：
~~~shell
make Workflow_DIR=/search/ted/1412/workflow_1412
~~~

### 示例
我们以``dag_demo.cc``为例。

这个例子是构建一个a、b、c、d的菱形DAG。其中四个节点可以是任意一种任务，我们这里为了方便，用了``WFTimerTask``。

因为DAGTask需要承载一些数据，所以整个DAG的数据是它的模版，也就是例子中的``my_ctx``。

我们需要做的事情是：

- 准备好各个节点，比如a、b、c、d，节点可以从Workflow现有的协议创建，或者是自己的算法task；
- 创建一个DAGTask，可以传入回调函数，用法与workflow其他task一致；
- 通过DAGTask的接口``bool add_edge(SubTask *src, SubTask *dst)``把a、b、c、d逻辑依赖加上，也就是DAG的边。
- 把全局想要操作的ctx通过``void set_ctx(T *ctx)``接口挂上DAGTask。
- 每个任务可以通过``void add_preparation(SubTask *src, preparation_t preparation);``来传入准备函数，准备函数要做的事情是：这个节点执行前，需要从全局ctx拿什么数据下来；准备函数类型为：``std::function<void (SubTask *src, T *ctx)>;``
- 构建好DAG后，执行``start()``；
- 由于是异步任务，记得等回来之后再退出进程，所以我们的例子里用了一个``WFFacilities::WaitGroup``来卡住。


