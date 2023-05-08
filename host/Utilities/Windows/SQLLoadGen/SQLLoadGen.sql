
if (select count(*) from sys.tables where name = 'InMage_DummyTable') = 0
BEGIN
Create Table  InMage_DummyTable
(
	Time_Stamp   DateTime,
	First_Name char(50),
	Last_Name  char(50),
	city       char(50)
  
)
END

DECLARE @Counter INT
SET @Counter = 0 
--while(@Counter < 10)
while( 1>0)
BEGIN
	SET @Counter = @Counter + 1 
	Insert Into InMage_DummyTable
	(Time_Stamp,First_Name,Last_Name,city)
	 values (GetDate(),'InMage','Hyderabad','India')

	if @Counter = 4
	BEGIN
		SET @Counter = 0
		DECLARE @records INT
		SET @records = (SELECT count(*) from InMage_DummyTable)
		Insert Into InMage_DummyTable
		(Time_Stamp,First_Name,Last_Name,city)
		values(GetDate(),@records+1,@records+1,@records+1)
	END
END

SET @records = (SELECT count(*) from InMage_DummyTable)
Insert Into InMage_DummyTable
(Time_Stamp,First_Name,Last_Name,city) 
values(GetDate(),@records+1,@records+1,@records+1)
select * from InMage_DummyTable

