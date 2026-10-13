MATLAB Runtime v9.3（R2017b）离线安装器

零级成像的后处理程序依赖本运行库。安装器已随 SDK 提供，客户无需再从网络下载。

  runtime/MyAppInstaller_mcr.install   约 924MB，Linux x86_64
  runtime/install_mcr.sh               安装脚本

安装（默认目录 /usr/local/MATLAB/MATLAB_Runtime/v93）:

  sudo bash runtime/install_mcr.sh
  sudo bash runtime/install_mcr.sh --silent 建议使用这条命令

装到其他目录时:

  bash runtime/install_mcr.sh --dest /opt/MATLAB/MATLAB_Runtime

成像时 mcrRoot 填安装结果下的 v93 目录。引擎在 engine/for_testing，无需另装。
