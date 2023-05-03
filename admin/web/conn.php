<?
    include_once("config.php");
	include_once("retentionFunctions.php");

  
/*	
# Execution states
	$QUEUED = 0;
      $READY  = 1;
      $STARTED = 2;
   	$INPROGRESS = 3;
      $COMPLETED = 4;
      $FAILED    =5;
      $S_R_COMP_STARED =6;
      $S_R_COMP_PROGRESS =7;
       
# Snap Type
      $SCHED_SNAP_PHYSICAL = 0;
      $RECOVERY_PHYSICAL   = 1;
      $ROLLBACK            = 2;
      $SCHED_SNAP_VIRTUAL  = 4;
      $RECOVERY_VIRTUAL    = 5;
      $EVENT_BASED         = 6;

 VOLUME TYPE :::
	0-Physical
	2-virtual
	1-virtual mounted


*/
$conn_included = 1;
?>
