<?php
class XCache {
    
    private static $xcobj;
    
    /**
     * getInstance 
     * 
     * @static
     * @access public
     * @return object XCache instance
     */
    public function getInstance() 
    {
        if (!(self::$xcobj instanceof XCache)) {
            self::$xcobj = new XCache;
        }
        return self::$xcobj; 
    }

    /**
     * __set 
     * 
     * @param mixed $name 
     * @param mixed $value 
     * @access public
     * @return void
     */
	public function cache_add($name, $value)
    {
        xcache_set($name, $value);
    }
 
    /**
     * __get 
     * 
     * @param mixed $name 
     * @access public
     * @return void
     */
    public function cache_get($name)
    {
        return xcache_get($name);
    }

    /**
     * __isset 
     * 
     * @param mixed $name 
     * @access public
     * @return bool
     */
   public function cache_exists($name)
    {
        return xcache_isset($name);
    }
	
	public function cache_delete($name)
    {
        xcache_unset($name);
    }

}
?>