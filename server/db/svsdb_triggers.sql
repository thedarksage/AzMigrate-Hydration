
# ---------------------------------------------------------------------- #
# Repair/add triggers                                                    #
# ---------------------------------------------------------------------- #

delimiter ~

CREATE TRIGGER `Trg_sanInitiatorDeleteConstraintCheck` BEFORE DELETE on sanInitiators
FOR EACH ROW
BEGIN
DECLARE sPortWwn VARCHAR(255) default ' ';
select distinct(exportedInitiatorPortWwn) into sPortWwn from accessControlGroupPorts where exportedInitiatorPortWwn = OLD.sanInitiatorPortWwn;
IF (sPortWwn = OLD.sanInitiatorPortWwn) THEN
DELETE FROM accessControlGroupPorts WHERE exportedInitiatorPortWwn = OLD.sanInitiatorPortWwn;
END IF;
END;
~

delimiter ;

delimiter ~

CREATE TRIGGER `Trg_exportedDevicesKeyConstraint` BEFORE INSERT on exportedDevices
FOR EACH ROW BEGIN
  DECLARE lvDeviceName VARCHAR(255) default ' ';
  DECLARE snapDeviceName VARCHAR(255) default ' ';
  DECLARE schSnapDeviceName VARCHAR(255) default ' ';
  select distinct(deviceName) into lvDeviceName from logicalVolumes where hostId = NEW.hostId and deviceName = NEW.exportedDeviceName;
  select distinct(destDeviceName) into snapDeviceName from snapShots where destHostid = NEW.hostId and destDeviceName = NEW.exportedDeviceName;
  select distinct(destDeviceName) into schSnapDeviceName from snapshotMain where destHostid = NEW.hostId and destDeviceName = NEW.exportedDeviceName;
  IF ((lvDeviceName != NEW.exportedDeviceName and snapDeviceName != NEW.exportedDeviceName and schSnapDeviceName != NEW.exportedDeviceName)) THEN
    insert into edKeyConstraint set hostId = NEW.hostId, exportedDeviceName = NEW.exportedDeviceName;
  END IF;
END;
~

delimiter ;

delimiter ~

CREATE TRIGGER `Trg_exportedInitiatorsDeleteConstraintCheck` BEFORE DELETE on exportedInitiators
FOR EACH ROW
BEGIN
DECLARE sPortWwn VARCHAR(255) default ' ';
select distinct(exportedInitiatorPortWwn) into sPortWwn from accessControlGroupPorts where exportedInitiatorPortWwn = OLD.exportedInitiatorPortWwn;
IF (sPortWwn = OLD.exportedInitiatorPortWwn) THEN
DELETE FROM accessControlGroupPorts WHERE exportedInitiatorPortWwn = OLD.exportedInitiatorPortWwn;
END IF;
END;
~

delimiter ;

delimiter ~

CREATE TRIGGER `Trg_accessControlGroupPortKeyConstraintCheck` BEFORE INSERT on accessControlGroupPorts
FOR EACH ROW
BEGIN
  DECLARE sPortWwn VARCHAR(255) default ' ';
  DECLARE ePortWwn VARCHAR(255) default ' ';
  DECLARE aPortWwn VARCHAR(255) default ' ';
  select distinct(sanInitiatorPortWwn) into sPortWwn from sanInitiators where sanInitiatorPortWwn = NEW.exportedInitiatorPortWwn;
  select distinct(exportedInitiatorPortWwn) into ePortWwn from exportedInitiators where exportedInitiatorPortWwn = NEW.exportedInitiatorPortWwn;
  select distinct(portId) into aPortWwn from arraySanHostInfo where portId = NEW.exportedInitiatorPortWwn;
  IF (sPortWwn != NEW.exportedInitiatorPortWwn and ePortWwn != NEW.exportedInitiatorPortWwn and aPortWwn != NEW.exportedInitiatorPortWwn) THEN
     insert into eACGKeyConstraint set exportedInitiatorPortWwn = NEW.exportedInitiatorPortWwn;
  END IF;
END;
~

delimiter ;

# ---------------------------------------------------------------------- #
# Triggers                                                               #
# ---------------------------------------------------------------------- #

delimiter ~

CREATE TRIGGER `Trg_discoveredInitiatorsConstraintCheck` BEFORE DELETE on discoveredInitiators
FOR EACH ROW
BEGIN
DECLARE initiatorPortWwn VARCHAR(255) default ' ';
select discoveredInitiatorPortWwn into initiatorPortWwn from discoveredZoneITNexus where discoveredInitiatorPortWwn = OLD.discoveredPortWwn;

IF (initiatorPortWwn = OLD.discoveredPortWwn) THEN
DELETE FROM discoveredZoneITNexus where discoveredInItiatorPortWwn = OLD.discoveredPortWwn;

END IF;
END;
~

delimiter ;

delimiter ~

CREATE TRIGGER `Trg_discoveredTargetsConstraintCheck` BEFORE DELETE on discoveredTargets
FOR EACH ROW
BEGIN
DECLARE targetPortWwn VARCHAR(255) default ' ';
select discoveredTargetPortWwn into targetPortWwn from discoveredZoneITNexus where discoveredtargetPortWwn = OLD.discoveredPortWwn;

IF (targetPortWwn = OLD.discoveredPortWwn) THEN
DELETE FROM discoveredZoneITNexus where discoveredTargetPortWwn =  OLD.discoveredPortWwn;

END IF;
END;
~
delimiter ;

