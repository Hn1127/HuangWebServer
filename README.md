# HuangWebServer

注意使用open打开文件时，若使用的是相对路径，则是以运行程序时的路径为路径起点
如在 /webServer 目录下使用 /bin/server 命令启动程序，则相对路径的起点为 /webServer

create database HuangWebServer;

use HuangWebServer;

create table user_table(
    username char(20) PRIMARY KEY,
    password char(20)
);