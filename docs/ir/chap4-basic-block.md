# 第3章 跳转与CFG

## 前置知识

- [ ] 掌握最基础的类型系统
  - [ ] 知道 MYGL IR 编译时类型与 RTTI 枚举的区别
  - [ ] 知道 MYGL 中有哪些类型
    - [ ] 知道空类型 -- `void` (`MYGL::IRBase::VoidType`)
    - [ ] 知道最常用的类型 -- `IntType`
  - [ ] 了解类型之间三种不同强度的相等关系
  - [ ] 了解类型上下文 `TypeContext`
    - [ ] 知道每个类型都依附于一个类型上下文
    - [ ] 知道类型上下文的作用
    - [ ] 会使用类型上下文创建类型
- [ ] 知道顺序的 IR 结构

## 跳转语句

很显然，只有顺序结构是写不出复杂的功能的，在C语言和 SysY 中，我们还需要跳转、循环语句。例如，Jim Hall 是用一个猜数字游戏来教跳转的。

这是一个最简单的猜数字游戏：

```C
int guess_number(int real_number)
{
    int input = getint();

    if (input == real_number)
        return 0;  // 返回0, 表示成功
    else if (input >= real_number)
        return 1;  // 返回1, 表示太大了
    else
        return -1; // 返回-1, 表示太小了
}
```

倘若你查 MYGL 的源代码，或者是指令集手册，你应该能找到这两条指令：

```llvm
br i1 <cond>, label %<true>, label %<false>
br label %<target>
```



```C
int main()
{
    int left = 0, right = 100;
    puts("想一个数字(0-100), 按 <Enter> 让我猜猜你的");
    getchar();

    while (left < right) {
        int mid = (left + right) / 2;
        printf("我猜 [%d]. 大了(1), 小了(-1), 还是刚刚好(0)? ", mid);
        int result = getint();

        if (result == 1) {
            left = mid;
        } else if (result == 0) {
            puts("猜对了!");
            return 0;
        } else {
            right = mid;
        }
    }
    printf("那看来只能是 [%d] 了.\n", left);
    return 0;
}
```

