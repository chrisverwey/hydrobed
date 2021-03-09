var express = require('express'); // Web Framework
var app = express();
var sql = require('mssql'); // MS Sql Server client
var bodyParser = require('body-parser');
var moment = require('moment');

// Connection string parameters.
var config = require('./config');
var sqlConfig = config.sqlConfig;
//SQL options.useUTC

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ 
   extended: true 
}));

// create application/json parser
var jsonParser = bodyParser.json()

// create application/x-www-form-urlencoded parser
var urlencodedParser = bodyParser.urlencoded({ extended: false })

// Start server and listen on http://localhost:8081/
var server = app.listen(8081, function () {
    var host = server.address().address
    var port = server.address().port

    console.log("app listening at http://%s:%s", host, port)
});
//Date.prototype.toJSON = function(){ return moment(this.addHours(-2)).format("YYYY-MM-DDTHH:mm:ss:ms"); }

app.post('/controller/register', urlencodedParser, function (req, res) {
    sql.connect(sqlConfig, function() {	
		console.log(req.body);
        var request = new sql.Request();
        var stringRequest = 'select c.controller_id, '+
        	'c.checkin_delay, '+
        	'c.last_updated as schedule_time '+
			'from controller C '+
			'where mac_address=\''+ req.body.mac_address +'\' ';

        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
			res.end(JSON.stringify(recordset));
			var logging = new sql.Request();
			var datetime = new moment(new Date());
			var logRequest = 'insert into logmessage values (' + 
						'(select (1) controller_id from controller where mac_address=\'' + 
							req.body.mac_address + '\'),' +
						'\'' + new Date().addHours(2).toISOString().replace(/Z/,'') 
						+ '\', 1, \'Register:Uptime='+req.body.uptime+'\')';
			logging.query(logRequest, function(err2, recordset2) {
				if (err2) console.log(err2);
			});
        });
    });
})

app.post('/logmessage', urlencodedParser, function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert into logmessage values (' + 
				       req.body.controller + ',' +
        		'\'' + new Date().addHours(2).toISOString().replace(/Z/,'') + '\',	' +
        		       req.body.priority + ',' +
        		'\'' + req.body.message.replace(/\'/g,'"') + '\')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})


// DEPRECATED
app.post('/sensor', urlencodedParser, function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert into reading values (' + 
				       req.body.pinId + ',' +
        		'\'' + new Date().addHours(2).toISOString().replace(/Z/,'') + '\',	' +
        		       req.body.value+')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})
app.post('/reading', urlencodedParser, function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert into reading values (' + 
				       req.body.pinId + ',' +
        		'\'' + new Date().addHours(2).toISOString().replace(/Z/,'') + '\',	' +
        		       req.body.value+')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

// logmessage
// 	get # last 5 minutes
// 	get?from=nnnn&to=nnnn

app.get('/activation' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from activation';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/activation/:activationId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from activation where activation_id=' + req.params.activationId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/controller' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from controller';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/controller/:controllerId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from controller where controller_id=' + req.params.controllerId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/controller/:controllerId/driver' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select t.* from controller t2 '+
                  'join driver t on t.controller_id=t2.controller_id '+
                  'where t.controller_id= ' + req.params.controllerId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/controller/:controllerId/logmessage' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select t.* from controller t2 '+
                  'join logmessage t on t.controller_id=t2.controller_id '+
                  'where t.controller_id= ' + req.params.controllerId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/driver' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from driver';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/driver/:driverId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from driver where driver_id=' + req.params.driverId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/driver/:driverId/pin' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select t.* from driver t2 '+
                  'join pin t on t.driver_id=t2.driver_id '+
                  'where t.driver_id= ' + req.params.driverId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/logmessage' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from logmessage';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/logmessage/:logmessageId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from logmessage where logmessage_id=' + req.params.logmessageId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/pin' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from pin';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/pin/:pinId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from pin where pin_id=' + req.params.pinId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/pin/:pinId/activation' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select t.* from pin t2 '+
                  'join activation t on t.pin_id=t2.pin_id '+
                  'where t.pin_id= ' + req.params.pinId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/pin/:pinId/reading/latest' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select top 10 t.* from pin t2 '+
                  'join reading t on t.pin_id=t2.pin_id '+
                  'where t.pin_id= ' + req.params.pinId + 
                  ' order by reading_time desc';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/pin/:pinId/reading/latest/:count' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select top ' + req.params.count + ' t.* from pin t2 '+
                  'join reading t on t.pin_id=t2.pin_id '+
                  'where t.pin_id= ' + req.params.pinId +
                  ' order by reading_time desc';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/pin/:pinId/reading' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select t.* from pin t2 '+
                  'join reading t on t.pin_id=t2.pin_id '+
                  'where t.pin_id= ' + req.params.pinId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/reading' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from reading';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.get('/reading/:readingId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from reading where reading_id=' + req.params.readingId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
 app.put('/driver/:driverId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'update driver set '+
          'controller_id=' + req.body.controller_id + ',' + 
          'driver_type=' + req.body.driver_type + ',' + 
          'i2c_port=' + req.body.i2c_port + ',' + 
          'location=\'' + req.body.location + '\'' + ',' + 
          'name=\'' + req.body.name + '\'' + ',' + 
          'schedule_read_freq=' + req.body.schedule_read_freq + 
             'where driver_id=' + req.params.driverId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
 
 app.put('/activation/:activationId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'update activation set '+
          'duration=' + req.body.duration + ',' + 
          'end_time=\'' + req.body.end_time + '\'' + ',' + 
          'pin_id=' + req.body.pin_id + ',' + 
          'start_time=\'' + req.body.start_time + '\'' + 
             ' where activation_id=' + req.params.activationId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.put('/reading/:readingId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'update reading set '+
          'pin_id=' + req.body.pin_id + ',' + 
          'reading=' + req.body.reading + ',' + 
          'reading_time=\'' + req.body.reading_time + '\'' + 
             ' where reading_id=' + req.params.readingId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.put('/controller/:controllerId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'update controller set '+
          'checkin_delay=' + req.body.checkin_delay + ',' + 
          'last_updated=\'' + req.body.last_updated + '\'' + ',' + 
          'location=\'' + req.body.location + '\'' + ',' + 
          'mac_address=\'' + req.body.mac_address + '\'' + ',' + 
          'name=\'' + req.body.name + '\'' + 
             ' where controller_id=' + req.params.controllerId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.put('/driver/:driverId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'update driver set '+
          'controller_id=' + req.body.controller_id + ',' + 
          'driver_type=' + req.body.driver_type + ',' + 
          'i2c_port=' + req.body.i2c_port + ',' + 
          'location=\'' + req.body.location + '\'' + ',' + 
          'name=\'' + req.body.name + '\'' + ',' + 
          'schedule_read_freq=' + req.body.schedule_read_freq + 
             ' where driver_id=' + req.params.driverId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.put('/pin/:pinId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'update pin set '+
          'alert_high=' + req.body.alert_high + ',' + 
          'alert_low=' + req.body.alert_low + ',' + 
          'driver_id=' + req.body.driver_id + ',' + 
          'name=\'' + req.body.name + '\'' + ',' + 
          'pin_number=' + req.body.pin_number + ',' + 
          'pin_type=' + req.body.pin_type + ',' + 
          'warn_high=' + req.body.warn_high + ',' + 
          'warn_low=' + req.body.warn_low + 
             ' where pin_id=' + req.params.pinId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.put('/logmessage/:logmessageId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'update logmessage set '+
          'controller_id=' + req.body.controller_id + ',' + 
          'logtime=\'' + req.body.logtime + '\'' + ',' + 
          'message=\'' + req.body.message + '\'' + ',' + 
          'priority=' + req.body.priority + 
             ' where logmessage_id=' + req.params.logmessageId ;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
 
app.post('/activation' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert activation ' +
                '(end_time,pin_id,start_time) '+
             'values (' + '\'' + req.body.end_time + '\'' + ',' + 
                     req.body.pin_id + ',' + 
                     '\'' + req.body.start_time + '\'' + ')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.post('/controller' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert controller ' +
                '(checkin_delay,last_updated,location,mac_address,name) '+
             'values (' + req.body.checkin_delay + ',' + 
                     '\'' + req.body.last_updated + '\'' + ',' + 
                     '\'' + req.body.location + '\'' + ',' + 
                     '\'' + req.body.mac_address + '\'' + ',' + 
                     '\'' + req.body.name + '\'' + ')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.post('/driver' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert driver ' +
                '(controller_id,driver_type,i2c_port,location,name,schedule_read_freq) '+
             'values (' + req.body.controller_id + ',' + 
                     req.body.driver_type + ',' + 
                     req.body.i2c_port + ',' + 
                     '\'' + req.body.location + '\'' + ',' + 
                     '\'' + req.body.name + '\'' + ',' + 
                     req.body.schedule_read_freq + ')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.post('/pin' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert pin ' +
                '(alert_high,alert_low,driver_id,name,pin_number,pin_type,warn_high,warn_low) '+
             'values (' + req.body.alert_high + ',' + 
                     req.body.alert_low + ',' + 
                     req.body.driver_id + ',' + 
                     '\'' + req.body.name + '\'' + ',' + 
                     req.body.pin_number + ',' + 
                     req.body.pin_type + ',' + 
                     req.body.warn_high + ',' + 
                     req.body.warn_low + ')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.delete('/activation/:activationId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'delete from activation where activation_id=' + req.params.activationId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.delete('/reading/:readingId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'delete from reading where reading_id=' + req.params.readingId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.delete('/controller/:controllerId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'delete from controller where controller_id=' + req.params.controllerId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.delete('/driver/:driverId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'delete from driver where driver_id=' + req.params.driverId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.delete('/pin/:pinId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'delete from pin where pin_id=' + req.params.pinId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })
app.delete('/logmessage/:logmessageId' , function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'delete from logmessage where logmessage_id=' + req.params.logmessageId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
          if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
 })