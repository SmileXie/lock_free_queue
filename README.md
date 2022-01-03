# Repo Description
a lock-free queue developed by pure C.
C语言构建的无锁队列。

## 接口准备

C语言在C11的标准中，加入了原子操作的标准头文件`
stdatomic.h`，这为我们的无锁编程提供了很大的方便。C11的gcc 4.7及以上版本中支持，因此我们需要准备4.7版本以上的gcc。

C11为我们提供了一组封装好的CAS接口：（https://en.cppreference.com/w/c/atomic/atomic_compare_exchange）

```cpp
_Bool atomic_compare_exchange_strong( volatile A* obj,
                                      C* expected, C desired );
_Bool atomic_compare_exchange_weak( volatile A *obj,
                                    C* expected, C desired );
```

这一组CAS接口，比较\*obj与\*expected是否相等，如果相等，则将\*desired赋值给\*obj，并返回true；否则返回false。也就是“原子地”执行以下逻辑。

```cpp
if (memcmp(obj, expected, sizeof *obj) == 0) {
    memcpy(obj, &desired, sizeof *obj);
    return true;
} else {
    memcpy(expected, obj, sizeof *obj);
    return false;
}
```

这一组接口有strong和weak两个版本。

* weak：即使\*obj == \*expected，有时会“虚假地”返回false。带来的好处是有更高的性能。
* strong：返回值完全取决于\*obj与\*expected是否相等。

## 简要设计

无锁队列的数据结构如下图所示。

* 设计`struct lf_queue_head`用于存储队头，队尾，它的node成员，分别指向队头和队尾。
* `struct lf_queue_node`表示队列节点，其next指针指向队尾方向的下一节点
* `struct lf_queue_node`和`struct lf_queue_head`都有aba成员，用于操作计数统计以避免ABA问题。

![无锁队列数据结构](https://xs-upload.oss-cn-hangzhou.aliyuncs.com/img/lf_queue.png)

### 入队操作

新节点创建好之后，执行两个CAS：

* 将队头node的next指针指向新加入的节点。
* 将queue_t.tail.node队尾指针指向新加入的节点

# 出队操作

执行一个CAS：

* 把quque_t.head.node队头指针，指向第二个节点

队头指针原来指向的节点，即是出队的节点。

