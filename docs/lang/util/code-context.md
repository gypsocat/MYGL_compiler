# MYGL-C的源代码管理

---

MYGL-C计划支持多种源码输入形式——包括字符串、文件流等等，还要负责存储源码文件解析出的AST。既然如此，std::istream显然是满足不了这个需求的，所以MYGL-C使用的是自己的源码管理类 -- `CodeContext`(源码上下文).

`CodeContext`是MYGL-C前端的中转站，有输入流管理、源码存储、AST管理这几个功能。由于要保证可重入性，MYGL-C没有全局的源码上下文，所有的源码上下文都要主程序(或者叫"驱动器")自己管理。

`CodeContext`的类信息可能会以Doxygen文档的格式上传。

## 类的构造与析构

由于`CodeContext`存在参数类型完全一致但是含义不同的构造方法，而C++又没有像Vala那样的具名构造函数，所以它的所有构造函数都是被屏蔽掉的。`CodeContext`只能通过几个"静态工厂函数"在堆上构造，通过`CodeContext::PtrT`智能指针访问。

`CodeContext`的"静态工厂函数"如下：

- `static PtrT from_string(std::string const &str)`
- `static PtrT from_filename(std::string const &filename)`
- `static PtrT from_input_file(std::istream const &stream = std::cin)`

## 成员

`CodeContext`自带一个类型为`yyFlexLexer`的词法分析器`lexer`，负责lexer的生命周期。本来打算把`lexer`交给`Parser`管的，但是由于`yyFlexLexer`自带一个字符缓冲区，而语法树结点又需要一个固定的字符缓冲区来表示自己的范围，所以必须保证`lexer`在`CodeContext`析构时一同析构。
