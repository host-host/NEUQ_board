如无特殊说明，你不能在其他目录写，包括写入/tmp等目录。
对于NEUQ_board项目：
第一次接手后端时建议从 Makefile 开始理解项目框架
第一次接手前端时建议直接看 www 文件夹，这是网站根路径
如有需要查生产数据的，建议直接使用 ndb2_init_readonly 查询
如有需要查网站访问记录的，建议先看 /etc/nginx/nginx.conf 