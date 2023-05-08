#ifndef REGISTERMT__H
#define REGISTERMT__H


namespace MTRegistration
{
	// registerMT 
	//  fetches MARS agent cert and registry keys from CS
	//  imports the cert into local machine cert store
	//  imports the registry keys into registry under MARS agent config
	bool registerMT();
};

#endif
