-- Generate a SELECT ALL RECORDS (DANGEROUS - USE RANGES RATHER, SOME TABLES ARE BIG)
select 'app.get(''/' + name + ''' , function (req, res) {' + 
'§    sql.connect(sqlConfig, function() {	' +
'§        var request = new sql.Request();' +
'§        var stringRequest = ''select * from ' + name + ''';' +
'§        console.log(stringRequest);' +
'§        request.query(stringRequest, function(err, recordset) {' +
'§          if(err) console.log(err);' +
'§            res.end(JSON.stringify(recordset)); // Result in JSON format' +
'§        });' +
'§    });' +
'§ })'  as JS
from sys.all_objects t where type='U' and  schema_id = 1
UNION
-- Generate a SELECT BY PRIMARY KEY
select 'app.get(''/' + name + '/:' + name + 'Id'' , function (req, res) {' + 
'§    sql.connect(sqlConfig, function() {	' +
'§        var request = new sql.Request();' +
'§        var stringRequest = ''select * from ' + name + ' where '+
    (select top 1 col.name 
     from sys.indexes pk
     inner join sys.index_columns ic
        on ic.object_id = pk.object_id
        and ic.index_id = pk.index_id
     inner join sys.columns col
        on pk.object_id = col.object_id
        and col.column_id = ic.column_id
     where t.object_id = pk.object_id 
     and pk.is_primary_key=1) +
'='' + req.params.' + name + 'Id;' +
'§        console.log(stringRequest);' +
'§        request.query(stringRequest, function(err, recordset) {' +
'§          if(err) console.log(err);' +
'§            res.end(JSON.stringify(recordset)); // Result in JSON format' +
'§        });' +
'§    });' +
'§ })' as JS
from sys.all_objects t where type='U' and  schema_id = 1
UNION
-- Generate a SELECT BY LINKED TABLE
select 'app.get(''/' + t2.name + '/:' + t2.name + 'Id/' + t.name + ''' , function (req, res) {' + 
'§    sql.connect(sqlConfig, function() {	' +
'§        var request = new sql.Request();' +
'§        var stringRequest = ''select t.* from ' + t2.name + ' t2 ''+' + 
'§                  ''join '+t.name + ' t on t.' + pc.name + '=t2.'+rc.name+' ''+' +
'§                  ''where t.' + pc.name + '= '' + req.params.' + t2.name + 'Id ;' +
'§        console.log(stringRequest);' +
'§        request.query(stringRequest, function(err, recordset) {' +
'§          if(err) console.log(err);' +
'§            res.end(JSON.stringify(recordset)); // Result in JSON format' +
'§        });' +
'§    });' +
'§ })'  as JS
from sys.tables t 
inner join sys.foreign_key_columns fk on t.object_id = fk.parent_object_id --and pk.is_primary_key=0
inner join sys.tables t2 on t2.object_id = fk.referenced_object_id
inner join sys.columns rc on fk.referenced_column_id = rc.column_id and fk.referenced_object_id = rc.object_id
inner join sys.columns pc on fk.parent_column_id = pc.column_id and fk.parent_object_id = pc.object_id
where t.schema_id = 1;
