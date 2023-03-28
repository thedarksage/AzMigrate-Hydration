#ifndef HASHCONVERTOR__H_
#define HASHCONVERTOR__H_

#include "baseconvertor.h"
#include "hashcomparedata.h"
#include <boost/shared_ptr.hpp>

class HashConvertor
{
private:
    BaseConvertor* m_convertor ;
    
    HashConvertor(void);
public:
    HashConvertor( BaseConvertor* convertor ) ;
    ~HashConvertor(void);

    void convertNode( HashCompareNode& node ) ;
    void convertInfo( HashCompareInfo& info ) ;
     
     BaseConvertor* getBaseConvertor() const { return m_convertor ; }    
};

typedef boost::shared_ptr<HashConvertor>  HashConvertorPtr ;


#endif /* HASHCONVERTOR__H_ */
