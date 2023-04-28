<?php
if($plan_type == 'BULK')
{
	$app_config['step']['primary_server_selection'] = 'primary_server_selection_other.php';
	$app_config['step']['secondary_server_selection'] = 'secondary_server_selection_other.php';
	$app_config['step']['protection_plan_review'] = 'protection_plan_review_other.php';
}
else
{
	$app_config['step']['primary_server_selection'] = 'primary_server_selection.php';
	$app_config['step']['secondary_server_selection'] = 'secondary_server_selection.php';
	$app_config['step']['protection_plan_review'] = 'protection_plan_review.php';
}

$app_config['step']['replication_options'] = 'replication_options.php';
$app_config['step']['process_server'] = 'select_ps.php';
$app_config['step']['retention_policy'] = 'retention_policy.php';
$app_config['step']['consistency_policy'] = 'app_consistency_policy.php';
$app_config['step']['configuration_change_policy'] = 'configure_change_policy.php';
$app_config['step']['readiness_check'] = 'readiness_check.php';
$app_config['step']['reverse_replication_options'] = 'reverse_replication.php';
$app_config['step']['volume_provisioning'] = 'volume_provisioning.php';
$app_config['step']['profiler_options'] = 'profiler_options.php';
$app_config['step']['protection_plan_review_profiler'] = 'protection_plan_review_profiler.php';

$app_config['step']['pause_plan'] = 'pause_plan_other.php';
$app_config['step']['restart_resync'] = 'pause_plan_other.php';

$app_config['step']['primary_server_selection_other'] = 'primary_server_selection_other.php';
$app_config['step']['secondary_server_selection_other'] = 'secondary_server_selection_other.php';
// For Exchange
$app_config['exchange']['default_consistency_option'] = '';


// PILLAR

$app_config['PILLAR']['protection_workflow']['primary_server_selection'] = 'array_primary_selection.php';
$app_config['PILLAR']['protection_workflow']['secondary_server_selection'] = 'array_secondary_selection.php';
$app_config['PILLAR']['protection_workflow']['replication_options'] = 'array_replication_options.php';
$app_config['PILLAR']['protection_workflow']['retention_policy'] = 'array_retention_policy.php';
$app_config['PILLAR']['protection_workflow']['reverse_replication_options'] = 'array_reverse_replication.php';
$app_config['PILLAR']['protection_workflow']['readiness_check'] = 'array_readiness_check.php';
$app_config['PILLAR']['protection_workflow']['protection_plan_review'] = 'array_plan_review.php';


?>