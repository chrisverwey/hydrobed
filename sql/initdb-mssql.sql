alter table activation drop CONSTRAINT	fk_pin_activation;
alter table driver drop CONSTRAINT fk_controller_driver;
alter table logmessage drop CONSTRAINT fk_controller_logmessage;
alter table pin drop CONSTRAINT fk_driver_pin;
alter table reading drop CONSTRAINT fk_pin_reading;

drop table if exists controller;
drop table if exists logmessage;
drop table if exists activation;
drop table if exists pin;
drop table if exists driver;
drop table if exists reading;

CREATE TABLE controller (
	controller_id   INTEGER IDENTITY (1,1) PRIMARY KEY,
	name			VARCHAR(30) NOT NULL,
	mac_address		VARCHAR(20) NOT NULL,
	address         VARCHAR(20) NOT NULL,
	checkin_delay	NUMERIC(3) NOT NULL,
	location		VARCHAR(30) NOT NULL,
	last_updated    DATETIME NOT NULL
);
INSERT INTO controller VALUES ('HYDRO TEST BEDS', '40:F5:20:32:F7:3A','172.16.1.154', 10,'-33.8813 18.5518','02/13/2021 21:54:00');

CREATE TABLE driver (
	driver_id		INTEGER IDENTITY (1,1) PRIMARY KEY,
	controller_id	INTEGER NOT NULL,
	driver_type		INTEGER, -- 1=DRIVER, 2=INA219 (power sensor)
    i2c_port 		INTEGER NOT NULL,
	name			VARCHAR(20) NOT NULL,
	location		VARCHAR(20) NOT NULL,
	schedule_read_freq	INTEGER NOT NULL,
	CONSTRAINT fk_controller_driver
		FOREIGN KEY (controller_id)
		REFERENCES controller(controller_id)
);
INSERT INTO driver VALUES 
	(1,1,1,'MAIN','BACK YARD', 600), -- 10 minutes
	(1,2,69,'POWER','BACK YARD',60); -- 5 minutes (0x45)

CREATE TABLE pin (
	pin_id			INTEGER IDENTITY (1,1) PRIMARY KEY,
	driver_id		INTEGER NOT NULL,
	name			VARCHAR(30) NOT NULL,
	pin_number		INTEGER NOT NULL,
	pin_type		INTEGER NOT NULL, -- 1=motor, 2=sensor
	alert_high		INTEGER,
	alert_low		INTEGER,
	warn_high		INTEGER,
	warn_low		INTEGER,
	CONSTRAINT fk_driver_pin
		FOREIGN KEY (driver_id)
		REFERENCES driver(driver_id)
);
INSERT INTO pin VALUES 
	(1,'FINE BED PUMP'      , 5,1,NULL,NULL,NULL,NULL),
    (1,'COARSE BED PUMP'    , 6,1,NULL,NULL,NULL,NULL),
    (1,'SOIL BED PUMP'      , 7,1,NULL,NULL,NULL,NULL),
    (1,'FINE BED MOISTURE 1'  ,14,2,NULL,NULL,NULL,NULL),
    (1,'FINE BED MOISTURE 2'  ,15,2,NULL,NULL,NULL,NULL),
    (1,'COARSE BED MOISTURE 1',16,2,NULL,NULL,NULL,NULL),
    (1,'COARSE BED MOISTURE 2',17,2,NULL,NULL,NULL,NULL),
    (1,'SOIL BED MOISTURE 1'  ,18,2,NULL,NULL,NULL,NULL),
    (1,'SOIL BED MOISTURE 2'  ,19,2,NULL,NULL,NULL,NULL),
    (2,'BUS V'                , 1,2,NULL,NULL,NULL,NULL),
    (2,'SHUNT mV'             , 2,2,NULL,NULL,NULL,NULL),
    (2,'CURRENT mA'           , 3,2,NULL,NULL,NULL,NULL),
    (2,'POWER mW'             , 4,2,NULL,NULL,NULL,NULL);

create table logmessage (
	logmessage_id	INTEGER IDENTITY (1,1) PRIMARY KEY,
	controller_id	INTEGER NOT NULL,
	logtime			DATETIME NOT NULL,
	priority		INTEGER,
    message         VARCHAR(8000),
	CONSTRAINT fk_controller_logmessage
		FOREIGN KEY (controller_id)
		REFERENCES controller(controller_id)
);

insert into logmessage values 
		(1,'06/02/2021 23:01:00',1,'OK'),
		(1,'06/02/2021 23:01:05',1,'OK'),
		(1,'06/02/2021 23:01:10',1,'OK');

create table activation (
	activation_id	INTEGER IDENTITY (1,1) PRIMARY KEY,
	pin_id			INTEGER NOT NULL,
	start_time		TIME NOT NULL,
	end_time		TIME NOT NULL,
	CONSTRAINT	fk_pin_activation
		FOREIGN KEY (pin_id)
		REFERENCES pin(pin_id)
);
INSERT INTO activation VALUES 
		(1,'07:00:00','07:00:30'),
		(2,'07:01:00','07:01:30');
		-- (1,'06:55:00','06:55:30'),
		-- (1,'15:00:00','15:00:30'),
		-- (1,'15:02:00','15:02:30'),
		-- (2,'06:57:00','06:57:30'),
		-- (2,'06:55:00','06:55:30'),
		-- (2,'15:00:00','15:00:30'),
		-- (2,'15:02:00','15:02:30'),
		-- (3,'06:57:00','06:57:30'),
		-- (3,'06:55:00','06:55:30'),
		-- (3,'15:00:00','15:00:30'),
		-- (3,'15:02:00','15:02:30');

create table reading (
    reading_id      INTEGER IDENTITY (1,1) PRIMARY KEY,
    pin_id          INTEGER NOT NULL,
    reading_time    DATETIME NOT NULL,
    reading         FLOAT NOT NULL,
    CONSTRAINT fk_pin_reading
        FOREIGN KEY (pin_id)
        REFERENCES pin(pin_id)
);



