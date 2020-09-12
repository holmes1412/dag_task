workflow_inc = -I${Workflow_DIR}/_include
workflow_lib = -L${Workflow_DIR}/_lib

includes     = -I../_include $(workflow_inc)
libs         = -L../_lib $(workflow_lib)

all: info dag_demo

info:
ifeq ("${Workflow_DIR}workflow", "workflow")
	@echo "Please set \"Workflow_DIR\" to make dag_demo."
	exit 1
endif

dag_demo:
	g++ -o dag_demo dag_demo.cc \
		-g -O0 --std=c++11 $(includes) $(libs) \
		-lworkflow -lpthread -lcrypto -lssl
