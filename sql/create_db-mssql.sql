alter database FARMM
set single_user
with rollback immediate;
drop database FARMM;

CREATE DATABASE FARMM ON  PRIMARY 
(NAME = 'FARMM', FILENAME = '/var/opt/sqlserver/farmm.mdf')
 LOG ON 
( NAME = 'farmm_log', FILENAME = '/var/opt/sqlserver/farmm_log.ldf');