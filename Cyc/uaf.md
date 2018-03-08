需要检查的有：
- 函数的参数中是否有指针，若有指针，则需要提前对其的metadata进行赋值
- 函数的返回值是否为指针，若有指针，！！！！！！！！！！！！！！！
- 函数中是否有malloc或free函数

需要处理的有：
- malloc
- free
- alloc
- load 的时候检查指针读取
- store 的时候检查指针解引用是否合法
- bitcast add等一系列需要进行指针操作的，进行指针的元数据的扩散
- 在main函数开始的时候，插入初始化的代码

所有指针的处理，传入的全部为指针的地址