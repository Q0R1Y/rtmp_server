#ifndef RSS_CORE_AUTO_FREE_HPP
#define RSS_CORE_AUTO_FREE_HPP

/*
#include <rss_core_auto_free.hpp>
*/

#include <rss_core.hpp>

/**
* auto free the instance in the current scope.
*/
#define RssAutoFree(className, instance, is_array) \
	__RssAutoFree<className> _auto_free_##instance(&instance, is_array)
    
template<class T>
class __RssAutoFree
{
private:
    T** ptr;
    bool is_array;
public:
    /**
    * auto delete the ptr.
    * @is_array a bool value indicates whether the ptr is a array.
    */
    __RssAutoFree(T** _ptr, bool _is_array){
        ptr = _ptr;
        is_array = _is_array;
    }
    
    virtual ~__RssAutoFree(){
        if (ptr == NULL || *ptr == NULL) {
            return;
        }
        
        if (is_array) {
            delete[] *ptr;
        } else {
            delete *ptr;
        }
        
        *ptr = NULL;
    }
};


#endif