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

app.get('/controller/:controllerId/driver', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select d.driver_id, driver_type, i2c_port, schedule_read_freq, count(p.pin_id) as pin_count ' +
				'from driver d '+
				'join pin p on p.driver_id = d.driver_id ' +
				'where controller_id = ' + req.params.controllerId + ' ' + 
				'group by d.driver_id, driver_type, i2c_port, schedule_read_freq;'
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

// driver config
app.get('/driver/:driverId/pin', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select pin_id, pin_number, pin_type, alert_high, alert_low, warn_high, warn_low ' +
		        ' from pin where driver_id = '+req.params.driverId;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/pin/:pinId/schedule', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select activation_id, start_time, end_time ' +
		        'from pin p ' +
		        'join activation a on a.pin_id = p.pin_id ' +
		        'where p.pin_id = '+req.params.pinId;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

// controller
// 	get
// 	get/?
// 	put/?
// #	post
// 	delete/?
// logmessage
// 	get # last 5 minutes
// 	get?from=nnnn&to=nnnn
// 	get/controller/? # last 5 minutes
// 	get/controller/? # last 5 minutes
// 	get/controller/n?from=nnnn&to=nnnn
// #	post/?
// 	delete/?
// 	X put
// activation
// 	get/?
// 	get/pin/? #gives all
// 	get/pin/?/latest #gives latest
// 	put/?
// 	post
// pin
// 	get/?
// 	get/driver/?
// 	put/?
// 	post
// 	delete/?
// driver
// 	get/?
// 	get/controller/?
// 	put/?
// 	post
// 	delete/?
// reading
// 	get/?
// 	get/controller/?
// 	get/driver/?
// 	get/pin/?
// #	post
// 	delete/?



