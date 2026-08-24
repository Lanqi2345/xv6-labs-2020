# xv6 Labs 2020

本仓库用于记录 MIT 6.S081 / xv6 2020 操作系统实验的实现代码。

各实验分别保存在不同的 Git 分支中，`main` 分支用于存放项目说明和实验文档。

## 实验分支

| 分支 | 实验内容 |
| --- | --- |
| `util` | Xv6 and Unix utilities |
| `syscall` | System calls |
| `pgtbl` | Page tables |
| `traps` | Traps |
| `lazy` | xv6 lazy page allocation |
| `cow` | Copy-on-Write Fork for xv6 |
| `thread` | Multithreading |
| `lock` | Lock |
| `fs` | File system |
| `mmap` | Memory mapping |
| `net` | Network driver |

## 查看分支

查看本地分支：

```bash
git branch
```

查看本地和远程的所有分支：

```bash
git branch -a
```

更新远程分支信息：

```bash
git fetch origin
```

切换到已有的本地分支：

```bash
git switch net
```

首次切换到只有远程存在的分支：

```bash
git switch --track origin/net
```

也可以直接在 GitHub 仓库页面左上方的分支菜单中选择需要查看的分支。

## 运行实验

切换到对应实验分支后，运行：

```bash
make qemu
```

退出 xv6：先按 `Ctrl+A`，再按 `X`。

运行自动测试：

```bash
make grade
```

## 更新代码

提交当前分支的修改：

```bash
git add .
git commit -m "描述本次修改"
git push
```

请在提交前确认当前所在分支：

```bash
git branch --show-current
```

## 项目说明

本仓库用于课程学习和实验记录。xv6 原始代码及实验材料的版权归其原作者所有。