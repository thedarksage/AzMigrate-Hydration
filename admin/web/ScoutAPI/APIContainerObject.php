<?php

abstract class APIContainerObject extends APIObject {
	abstract protected function getChildList();
	abstract protected function addChild($obj);

}

?>