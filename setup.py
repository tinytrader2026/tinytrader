from setuptools import setup, find_packages
from wheel.bdist_wheel import bdist_wheel
import platform

class bdist_wheel_abi(bdist_wheel):
    def get_tag(self):
        return ('py3', 'none', 'win_amd64') if platform.system() == "Windows" else ('py3', 'none', 'linux_x86_64')

setup(
    packages=find_packages(where="python"),
    package_dir={"": "python"},
    package_data={
        "tinytrader": ["*.so", "*.pyd", "*.dll", "*/*.so", "*/*.pyd"],
    },
    include_package_data=True,
    zip_safe=False,
    cmdclass={'bdist_wheel': bdist_wheel_abi},
)