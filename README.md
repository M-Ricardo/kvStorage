# kvStorage

# 一、项目主要功能和知识点
参照redis，主要实现的功能：  
1、数据的插入、查询、删除等操作  
1）SET：插入key - value  
2）GET：获取key对应的value  
3）COUNT：统计已插入多少个key  
4）DELETE：删除key以及对应的value  
5）EXIST：判断key是否存在  

2、实现不同的数据结构存储引擎  
不同的数据结构，操作的效率是不一样的。因此我们实现基于数组array、红黑树rbtree、哈希hash、跳表skiptable 这四种数据结构，实现kv存储，并测试相应的性能。  

3、测试用例  
1）功能测试  
2）10w的qps测试  
 
主要涉及的知识点有  
1）基于协程，一个连接对应一个协程  
2）tcp网络交互  
3）数据结构：数组array、红黑树rbtree、哈希hash、跳表skiptable、动态哈希dhash 

# 二、架构设计
![image](https://github.com/M-Ricardo/kvStorage/assets/49547846/65752bb7-9b59-4608-991f-1796faad6163)
![image](https://github.com/M-Ricardo/kvStorage/assets/49547846/37ca80ad-e61e-4c86-b6d1-2e414008b3cd)
![image](https://github.com/M-Ricardo/kvStorage/assets/49547846/d07e3fad-248c-44f9-8ace-b871e19cf309)

# 三、编译指令
进入到Ntyco
```c
cd Ntyco
make
```
返回主目录
```c
gcc -o kvstore kvstore.c -I ./NtyCo/core/ -L ./NtyCo/ -lntyco -lpthread -ldl 
```

启动两个xshell窗口
一个运行
```c
./kvstore
```
另一个运行
```c
gcc test.c -o test
./test 192.168.3.128 9999
```
