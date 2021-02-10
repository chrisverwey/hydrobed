drop table controller;
drop table driver;
drop table pin;
drop table logmessage;
drop table activation;

CREATE TABLE controller (
	controller_id   ID 	PRIMARY KEY,
	name			VARCHAR2(30) NOT NULL,
	mac_address		NUMERIC(6) NOT NULL,
	checkin_delay	NUMERIC(3) NOT NULL,
	uptime			NUMERIC(6),
	schedule_time	DATETIME,
	location		VARCHAR2(20) NOT NULL
);
INSERT INTO controller VALUES (1,'HYDRO TEST BEDS', 'MAC-TBC',60,NULL,NULL,'BACK YARD');

CREATE TABLE driver (
	driver_id		ID PRIMARY KEY,
	controller_id	INTEGER NOT NULL,
	name			VARCHAR2(20) NOT NULL,
	location		VARCHAR2(20) NOT NULL,
	schedule_read_freq	INTEGER NOT NULL,
	CONSTRAINT fk_controller_driver
		FOREIGN KEY (controller_id)
		REFERENCES controller(controller_id)
);
INSERT INTO driver VALUES (1,1,'MAIN','BACK YARD', 600);
INSERT INTO driver VALUES (2,1,'POWER','BACK YARD',300);

CREATE TABLE pin (
	pin_id			ID PRIMARY KEY,
	driver_id		INTEGER NOT NULL,
	name			VARCHAR2(30) NOT NULL,
	pin_number		INTEGER NOT NULL,
	pin_type		INTEGER NOT NULL,
--	schedule		INTEGER,
	alert_high		INTEGER,
	alert_low		INTEGER,
	warn_high		INTEGER,
	warn_low		INTEGER,
	CONSTRAINT fk_driver_pin
		FOREIGN KEY (driver_id)
		REFERENCES driver(driver_id)
);
INSERT INTO pin VALUES (1,1,'FINE BED PUMP'      , 5,1,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (2,1,'COARSE BED PUMP'    , 6,1,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (3,1,'SOIL BED PUMP'      , 7,1,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (4,1,'FINE BED MOISTURE'  ,14,2,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (5,1,'FINE BED MOISTURE'  ,15,2,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (6,1,'COARSE BED MOISTURE',16,2,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (7,1,'COARSE BED MOISTURE',17,2,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (8,1,'SOIL BED MOISTURE'  ,18,2,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (9,1,'SOIL BED MOISTURE'  ,19,2,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (10,2,'BATTERY VOLTS'     , 1,2,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (11,2,'BATTERY AMPS'      , 2,2,NULL,NULL,NULL,NULL);
INSERT INTO pin VALUES (12,2,'BATTERY WATTS'     , 3,2,NULL,NULL,NULL,NULL);

create table logmessage (
	logmessage_id	ID PRIMARY KEY,
	controller_id	INTEGER NOT NULL,
	logtime			DATETIME NOT NULL,
	priority		INTEGER,
	CONSTRAINT fk_controller_logmessage
		FOREIGN KEY (controller_id)
		REFERENCES controller(controller_id)
);
create table activation (
	activation_id	ID PRIMARY KEY,
	pin_id			INTEGER NOT NULL,
	start_time		DATETIME NOT NULL,
	duration		INTEGER NOT NULL,
	CONSTRAINT	fk_pin_activation
		FOREIGN KEY (pin_id)
		REFERENCES pin(pin_id)
);
INSERT INTO activation VALUES (1,1,'06:55:00',30);
INSERT INTO activation VALUES (2,1,'06:57:00',30);
INSERT INTO activation VALUES (3,1,'15:00:00',30);
INSERT INTO activation VALUES (4,1,'15:02:00',30);
