<?php
/**
 * Returns the Template for given name
 */
class TemplateFactory {
	
	public static function getTemplate($templateName) {
		$class = $templateName.'php';
		$template = new $templateName();
		return $template;
	}
}
?>