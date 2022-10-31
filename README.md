# shark简介
*  shark是一个高性能的协程网络组件, 可以使用同步的方式编写高性能异步的网络程序.
*  shark适合需要使用epoll处理大量并发tcp连接的场景，性能与裸用epoll相当.
*  shark在不降低性能的基础上，大大降低使用C++编写高性能网络程序的难度，使用简单的几个接口就可以构建高性能的网络程序.

# shark特性
*	同步无锁方式编写高性能异步代码
*	支持多核, 支持核心绑定
*	支持多进程并行计算, 支持进程间负载均衡
*	Hook并优化阻塞的网络系统调用, 让第三方组件同步变异步
*	支持动态协程池, 汇编实现的高性能协程上下文切换, 每秒达1.7亿次
*	高效的超时机制, 支持千万级timer
*	高性能的缓存机制


# shark原理
*   1) https://www.icloud.com/keynote/AwBWCAESEPgQD_NG7iyzkN4cnfn2xz4aKs0eZHgJ9yypoEO1ydG4SkrtCv03nfK84AAPbb7Hi4_AExZ45oGUbOSEwwMCUCAQEEIA_xmo4vPc9xyVx8LVDl3yELLql9EvhBsCMg6zpi90Qj#汇编与过程
*   2) https://www.icloud.com/keynote/AwBWCAESEG55iVo9FJypIRQWVVB4-m0aKotmiNdUkLNONiSyMV6jwMJqqe_39hJn2ZRxB2Mn1_bqGhrXxgbPgx4s6QMCUCAQEEIAnsMb0BflExH98bVZovKw8LAyxONZCbJJUo6MITZkWI#协程
