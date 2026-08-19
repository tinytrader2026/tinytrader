### 从源码编译 Python 接口

1. 在 Linux 环境编译需要先安装 Python 开发包。Windows 可跳过此步骤，安装 Python 时已包含。
```bash
# AlmaLinux / RHEL / CentOS
sudo dnf install python3-devel python3-pip

# Ubuntu / Debian
sudo apt install python3-dev python3-pip
```

2. 编译 TinyTrader 的 Python 绑定
```bash
# 安装依赖（仅编译需要）
pip install nanobind

# 编译
mkdir build 
cd build
NB_DIR=$(python3 -c "import nanobind; import os; print(os.path.join(os.path.dirname(nanobind.__file__), 'cmake'))")
cmake .. -DPYTHON_BINDINGS=ON -DPython_EXECUTABLE=$(which python3) -Dnanobind_DIR=$NB_DIR
cmake --build . --config Release --target _tinytrader
```
编译成功后，在 python/tinytrader/ 目录下会生成 _tinytrader.pyd (Windows) 或 _tinytrader.so (Linux)。

3. 安装生成的 Python 包
```bash
# 回到代码目录
cd ..
# 开发模式安装
pip install -e .
# 验证
python -c "import tinytrader as tt; print('TinyTrader', tt.__version__)"
```
