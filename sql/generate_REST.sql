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
where t.schema_id = 1
UNION
-- Generate a UPDATE BY PRIMARY KEY
select 'app.put(''/' + t.name + '/:' + t.name + 'Id'' , function (req, res) {' + 
'§    sql.connect(sqlConfig, function() {	' +
'§        var request = new sql.Request();' +
'§        var stringRequest = ''update ' + t.name + ' set ''+' +
'§          ' + string_agg('''' + col.name + '=' + 
                  case st.name 
                     when 'varchar' then '\''' 
                     when 'datetime' then '\''' 
                     when 'time' then '\''' 
                     else ''
                  end +
               ''' + req.body.' + col.name + ' + ' +
                  case st.name 
                     when 'varchar' then '''\'''' + ' 
                     when 'datetime' then '''\'''' + ' 
                     when 'time' then '''\'''' + ' 
                     else ''
                  end 
               , ''','' + §          ') within group (order by col.name) + 
'§             '' where '+ 
                  (select top 1 col2.name 
                  from sys.indexes pk2
                  inner join sys.index_columns ic2
                     on ic2.object_id = pk2.object_id
                     and ic2.index_id = pk2.index_id
                  inner join sys.columns col2
                     on pk2.object_id = col2.object_id
                     and col2.column_id = ic2.column_id
                  where t.object_id = pk2.object_id 
                  and pk2.is_primary_key=1) +
               '='' + req.params.' + t.name + 'Id ;' +
'§        console.log(stringRequest);' +
'§        request.query(stringRequest, function(err, recordset) {' +
'§          if(err) console.log(err);' +
'§            res.end(JSON.stringify(recordset)); // Result in JSON format' +
'§        });' +
'§    });' +
'§ })' as JS
from sys.all_objects t 
join sys.columns col on col.object_id = t.object_id
join sys.types st on st.system_type_id = col.system_type_id
     where  type='U' and  t.schema_id = 1
and (select 1
     from sys.indexes pk
     inner join sys.index_columns ic
        on ic.object_id = pk.object_id
        and ic.index_id = pk.index_id
   --   inner join sys.columns col2
   --      on pk.object_id = col2.object_id
   --      and col2.column_id = ic.column_id
     where t.object_id = pk.object_id 
     and col.column_id = ic.column_id
     and pk.is_primary_key=1) is null
group by t.name, t.object_id
UNION
-- Generate a POST 
select 'app.post(''/' + t.name + ''' , function (req, res) {' + 
'§    sql.connect(sqlConfig, function() {	' +
'§        var request = new sql.Request();' +
'§        var stringRequest = ''insert ' + t.name + ' '' +' +
'§                ''(' + string_agg(col.name , ',') within group (order by col.name) + ') ''+' +
'§             ''values ('' + ' + string_agg( 
                  case st.name 
                     when 'varchar' then '''\'''' + ' 
                     when 'datetime' then '''\'''' + ' 
                     when 'time' then '''\'''' + ' 
                     else ''
                  end + 'req.body.' + col.name + '' +
                  case st.name 
                     when 'varchar' then ' + ''\''''' 
                     when 'datetime' then ' + ''\''''' 
                     when 'time' then ' + ''\''''' 
                     else ''
                  end 
               , ' + '','' + §                     ') within group (order by col.name) + ' + '')'';' +
'§        console.log(stringRequest);' +
'§        request.query(stringRequest, function(err, recordset) {' +
'§          if(err) console.log(err);' +
'§            res.end(JSON.stringify(recordset)); // Result in JSON format' +
'§        });' +
'§    });' +
'§ })' as JS
from sys.all_objects t 
join sys.columns col on col.object_id = t.object_id
join sys.types st on st.system_type_id = col.system_type_id
     where  type='U' and  t.schema_id = 1
and (select 1
     from sys.indexes pk
     inner join sys.index_columns ic
        on ic.object_id = pk.object_id
        and ic.index_id = pk.index_id
   --   inner join sys.columns col2
   --      on pk.object_id = col2.object_id
   --      and col2.column_id = ic.column_id
     where t.object_id = pk.object_id 
     and col.column_id = ic.column_id
     and pk.is_primary_key=1) is null
group by t.name, t.object_id;

