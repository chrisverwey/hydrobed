var express = require('express'); // Web Framework
var app = express();
var sql = require('mssql'); // MS Sql Server client
var bodyParser = require('body-parser');
var moment = require('moment');

// Connection string parameters.
var sqlConfig = {
    user: 'sa',
    password: 'P@ssw0rd',
    server: 'srvfedora2.ourhome.co.za',
    port: 4433,
    database: 'FARMM'
}
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
//Date.prototype.toJSON = function(){ return moment(this).tz('Africa/Johannesburg').format("YYYY-MM-DDTHH:mm:ss:ms"); }

app.get('/controller/:controllerId/', function (req, res) {
    sql.connect(sqlConfig, function() {
        var request = new sql.Request();
        var stringRequest = 'select * from controller where controller_id = ' + req.params.controllerId;
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/controller/:controllerId/configuration', function (req, res) {
    sql.connect(sqlConfig, function() {
        var request = new sql.Request();
		var stringRequest = 'select '+
                    '    driver.driver_id, driver.i2c_port, driver.schedule_read_freq, '+
                    '    pin.pin_id, pin.pin_number, '+
                    '    activation.activation_id, activation.start_time, activation.duration '+
                    'from driver '+
                    'join pin on pin.driver_id = driver.driver_id '+
                    'join activation on activation.pin_id = pin.pin_id ' +
                    'where controller_id = ' + req.params.controllerId;
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
        var stringRequest = 'select * from driver where controller_id = ' + req.params.controllerId;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/driver/:driverId/', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from driver d where d.driver_id = '+req.params.driverId;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/driver/:driverId/pin', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from pin where driver_id = '+req.params.driverId;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/pin/:pinId/', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from pin where pin_id = ' + req.params.pinId;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/controller/:controllerId/logmessage', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from logmessage where controller_id = ' + req.params.controllerId;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/pin/:pinId/activation', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from activation a' + 
        					' where a.pin_id = '  + req.params.pinId;
        					;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/activation/:activationId/', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from activation a' + 
        					' where a.activation_id = '  + req.params.activationId;
        					;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('/reading', function (req, res) {
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

app.post('/logmessage', urlencodedParser, function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert into logmessage values (' + 
				       req.body.controller + ',' +
        		'\'' + new Date().addHours(2).toISOString().replace(/Z/,'') + '\',	' +
        		       req.body.priority + ',' +
        		'\'' + req.body.message + '\')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.post('X/reading', urlencodedParser, function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'insert into reading values (' + 
				       req.body.pinid + ',' +
        		'\'' + req.body.timestamp + '+GMT2\',' +
        		       req.body.reading + ')';
        console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.post('/controller', urlencodedParser, function (req, res) {
    sql.connect(sqlConfig, function() {	
		console.log(req.body);
        var request = new sql.Request();
        var stringRequest = 'select c.controller_id, '+
        	'c.checkin_delay, '+
        	'max(activation.start_time) as schedule_time '+
			'from controller C '+
			'join driver on driver.controller_id = c.controller_id '+
			'join pin on pin.driver_id = driver.driver_id '+
			'join activation on activation.pin_id = pin.pin_id '+
			'group by c.controller_id, c.checkin_delay, c.schedule_time;'

        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
//            res.end(recordset.recordset[0].JSON); // Result in JSON format
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

app.get('/controller', function (req, res) {
	console.log(req.body);
    sql.connect(sqlConfig, function() {
        var request = new sql.Request();
        request.query('select * from controller', function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

app.get('x/logmessage/:logmessageId', function (req, res) {
    sql.connect(sqlConfig, function() {	
        var request = new sql.Request();
        var stringRequest = 'select * from logmessage where logmessage_Id = ' + req.params.logmessageId;
    	console.log(stringRequest);
        request.query(stringRequest, function(err, recordset) {
            if(err) console.log(err);
            res.end(JSON.stringify(recordset)); // Result in JSON format
        });
    });
})

