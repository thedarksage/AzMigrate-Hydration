<?php 
class WinMemCache
{
    private static $wcobj;
    
    /**
     * getInstance 
     * 
     * @static
     * @access public
     * @return object XCache instance
     */
    public function getInstance() 
    {
        if (!(self::$wcobj instanceof WinMemCache)) {
            self::$wcobj = new WinMemCache;
        }
        return self::$wcobj; 
    }


     // returns
     //   true: if successfully added
     //   falses: keyId exists or error
     public function cache_add($keyId, $data)
     {
         return wincache_ucache_add($keyId, $data);
     }

     // returns
     //   mixed type: values stored with keyId
     //   null: keyId does not exist or error
     public function cache_get($keyId)
     {
         return wincache_ucache_get($keyId);
     }

	 // returns
     //   true: if successfully deleted
     //   falses: on error
     public function cache_delete($keyId)
     {
         return wincache_ucache_delete($keyId);
     }
	   // returns
     //   true: if keyId found
     //   falses: if keyId not found or error
	 public function cache_exists($keyId)
	 {
		 return wincache_ucache_exists($keyId);
	 }
	 
};
?>