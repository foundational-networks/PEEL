//
// Generated file, do not edit! Created by nedtool 5.6 from inet/applications/tcpapp/GenericAppMsg.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include "GenericAppMsg_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp

namespace {
template <class T> inline
typename std::enable_if<std::is_polymorphic<T>::value && std::is_base_of<omnetpp::cObject,T>::value, void *>::type
toVoidPtr(T* t)
{
    return (void *)(static_cast<const omnetpp::cObject *>(t));
}

template <class T> inline
typename std::enable_if<std::is_polymorphic<T>::value && !std::is_base_of<omnetpp::cObject,T>::value, void *>::type
toVoidPtr(T* t)
{
    return (void *)dynamic_cast<const void *>(t);
}

template <class T> inline
typename std::enable_if<!std::is_polymorphic<T>::value, void *>::type
toVoidPtr(T* t)
{
    return (void *)static_cast<const void *>(t);
}

}

namespace inet {

// forward
template<typename T, typename A>
std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec);

// Template rule to generate operator<< for shared_ptr<T>
template<typename T>
inline std::ostream& operator<<(std::ostream& out,const std::shared_ptr<T>& t) { return out << t.get(); }

// Template rule which fires if a struct or class doesn't have operator<<
template<typename T>
inline std::ostream& operator<<(std::ostream& out,const T&) {return out;}

// operator<< for std::vector<T>
template<typename T, typename A>
inline std::ostream& operator<<(std::ostream& out, const std::vector<T,A>& vec)
{
    out.put('{');
    for(typename std::vector<T,A>::const_iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (it != vec.begin()) {
            out.put(','); out.put(' ');
        }
        out << *it;
    }
    out.put('}');

    char buf[32];
    sprintf(buf, " (size=%u)", (unsigned int)vec.size());
    out.write(buf, strlen(buf));
    return out;
}

Register_Class(InnerList)

InnerList::InnerList(const char *name, short kind) : ::omnetpp::cMessage(name, kind)
{
}

InnerList::InnerList(const InnerList& other) : ::omnetpp::cMessage(other)
{
    copy(other);
}

InnerList::~InnerList()
{
    delete [] this->innerArray;
}

InnerList& InnerList::operator=(const InnerList& other)
{
    if (this == &other) return *this;
    ::omnetpp::cMessage::operator=(other);
    copy(other);
    return *this;
}

void InnerList::copy(const InnerList& other)
{
    delete [] this->innerArray;
    this->innerArray = (other.innerArray_arraysize==0) ? nullptr : new unsigned int[other.innerArray_arraysize];
    innerArray_arraysize = other.innerArray_arraysize;
    for (size_t i = 0; i < innerArray_arraysize; i++) {
        this->innerArray[i] = other.innerArray[i];
    }
}

void InnerList::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::omnetpp::cMessage::parsimPack(b);
    b->pack(innerArray_arraysize);
    doParsimArrayPacking(b,this->innerArray,innerArray_arraysize);
}

void InnerList::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::omnetpp::cMessage::parsimUnpack(b);
    delete [] this->innerArray;
    b->unpack(innerArray_arraysize);
    if (innerArray_arraysize == 0) {
        this->innerArray = nullptr;
    } else {
        this->innerArray = new unsigned int[innerArray_arraysize];
        doParsimArrayUnpacking(b,this->innerArray,innerArray_arraysize);
    }
}

size_t InnerList::getInnerArrayArraySize() const
{
    return innerArray_arraysize;
}

unsigned int InnerList::getInnerArray(size_t k) const
{
    if (k >= innerArray_arraysize) throw omnetpp::cRuntimeError("Array of size innerArray_arraysize indexed by %lu", (unsigned long)k);
    return this->innerArray[k];
}

void InnerList::setInnerArrayArraySize(size_t newSize)
{
    unsigned int *innerArray2 = (newSize==0) ? nullptr : new unsigned int[newSize];
    size_t minSize = innerArray_arraysize < newSize ? innerArray_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        innerArray2[i] = this->innerArray[i];
    for (size_t i = minSize; i < newSize; i++)
        innerArray2[i] = 0;
    delete [] this->innerArray;
    this->innerArray = innerArray2;
    innerArray_arraysize = newSize;
}

void InnerList::setInnerArray(size_t k, unsigned int innerArray)
{
    if (k >= innerArray_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    this->innerArray[k] = innerArray;
}

void InnerList::insertInnerArray(size_t k, unsigned int innerArray)
{
    if (k > innerArray_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = innerArray_arraysize + 1;
    unsigned int *innerArray2 = new unsigned int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        innerArray2[i] = this->innerArray[i];
    innerArray2[k] = innerArray;
    for (i = k + 1; i < newSize; i++)
        innerArray2[i] = this->innerArray[i-1];
    delete [] this->innerArray;
    this->innerArray = innerArray2;
    innerArray_arraysize = newSize;
}

void InnerList::insertInnerArray(unsigned int innerArray)
{
    insertInnerArray(innerArray_arraysize, innerArray);
}

void InnerList::eraseInnerArray(size_t k)
{
    if (k >= innerArray_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = innerArray_arraysize - 1;
    unsigned int *innerArray2 = (newSize == 0) ? nullptr : new unsigned int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        innerArray2[i] = this->innerArray[i];
    for (i = k; i < newSize; i++)
        innerArray2[i] = this->innerArray[i+1];
    delete [] this->innerArray;
    this->innerArray = innerArray2;
    innerArray_arraysize = newSize;
}

class InnerListDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
    enum FieldConstants {
        FIELD_innerArray,
    };
  public:
    InnerListDescriptor();
    virtual ~InnerListDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(InnerListDescriptor)

InnerListDescriptor::InnerListDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::InnerList)), "omnetpp::cMessage")
{
    propertynames = nullptr;
}

InnerListDescriptor::~InnerListDescriptor()
{
    delete[] propertynames;
}

bool InnerListDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<InnerList *>(obj)!=nullptr;
}

const char **InnerListDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *InnerListDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int InnerListDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 1+basedesc->getFieldCount() : 1;
}

unsigned int InnerListDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_innerArray
    };
    return (field >= 0 && field < 1) ? fieldTypeFlags[field] : 0;
}

const char *InnerListDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "innerArray",
    };
    return (field >= 0 && field < 1) ? fieldNames[field] : nullptr;
}

int InnerListDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0] == 'i' && strcmp(fieldName, "innerArray") == 0) return base+0;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *InnerListDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "unsigned int",    // FIELD_innerArray
    };
    return (field >= 0 && field < 1) ? fieldTypeStrings[field] : nullptr;
}

const char **InnerListDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *InnerListDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int InnerListDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    InnerList *pp = (InnerList *)object; (void)pp;
    switch (field) {
        case FIELD_innerArray: return pp->getInnerArrayArraySize();
        default: return 0;
    }
}

const char *InnerListDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    InnerList *pp = (InnerList *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string InnerListDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    InnerList *pp = (InnerList *)object; (void)pp;
    switch (field) {
        case FIELD_innerArray: return ulong2string(pp->getInnerArray(i));
        default: return "";
    }
}

bool InnerListDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    InnerList *pp = (InnerList *)object; (void)pp;
    switch (field) {
        case FIELD_innerArray: pp->setInnerArray(i,string2ulong(value)); return true;
        default: return false;
    }
}

const char *InnerListDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

void *InnerListDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    InnerList *pp = (InnerList *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

Register_Class(TwoDInnerList)

TwoDInnerList::TwoDInnerList(const char *name, short kind) : ::omnetpp::cMessage(name, kind)
{
}

TwoDInnerList::TwoDInnerList(const TwoDInnerList& other) : ::omnetpp::cMessage(other)
{
    copy(other);
}

TwoDInnerList::~TwoDInnerList()
{
    for (size_t i = 0; i < innerArray_arraysize; i++)
        drop(&this->innerArray[i]);
    delete [] this->innerArray;
}

TwoDInnerList& TwoDInnerList::operator=(const TwoDInnerList& other)
{
    if (this == &other) return *this;
    ::omnetpp::cMessage::operator=(other);
    copy(other);
    return *this;
}

void TwoDInnerList::copy(const TwoDInnerList& other)
{
    for (size_t i = 0; i < innerArray_arraysize; i++)
        drop(&this->innerArray[i]);
    delete [] this->innerArray;
    this->innerArray = (other.innerArray_arraysize==0) ? nullptr : new InnerList[other.innerArray_arraysize];
    innerArray_arraysize = other.innerArray_arraysize;
    for (size_t i = 0; i < innerArray_arraysize; i++) {
        this->innerArray[i] = other.innerArray[i];
        this->innerArray[i].setName(other.innerArray[i].getName());
        take(&this->innerArray[i]);
    }
}

void TwoDInnerList::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::omnetpp::cMessage::parsimPack(b);
    b->pack(innerArray_arraysize);
    doParsimArrayPacking(b,this->innerArray,innerArray_arraysize);
}

void TwoDInnerList::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::omnetpp::cMessage::parsimUnpack(b);
    delete [] this->innerArray;
    b->unpack(innerArray_arraysize);
    if (innerArray_arraysize == 0) {
        this->innerArray = nullptr;
    } else {
        this->innerArray = new InnerList[innerArray_arraysize];
        doParsimArrayUnpacking(b,this->innerArray,innerArray_arraysize);
    }
}

size_t TwoDInnerList::getInnerArrayArraySize() const
{
    return innerArray_arraysize;
}

const InnerList& TwoDInnerList::getInnerArray(size_t k) const
{
    if (k >= innerArray_arraysize) throw omnetpp::cRuntimeError("Array of size innerArray_arraysize indexed by %lu", (unsigned long)k);
    return this->innerArray[k];
}

void TwoDInnerList::setInnerArrayArraySize(size_t newSize)
{
    InnerList *innerArray2 = (newSize==0) ? nullptr : new InnerList[newSize];
    size_t minSize = innerArray_arraysize < newSize ? innerArray_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        innerArray2[i] = this->innerArray[i];
    for (size_t i = 0; i < innerArray_arraysize; i++)
        drop(&this->innerArray[i]);
    delete [] this->innerArray;
    this->innerArray = innerArray2;
    innerArray_arraysize = newSize;
    for (size_t i = 0; i < innerArray_arraysize; i++)
        take(&this->innerArray[i]);
}

void TwoDInnerList::setInnerArray(size_t k, const InnerList& innerArray)
{
    if (k >= innerArray_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    this->innerArray[k] = innerArray;
}

void TwoDInnerList::insertInnerArray(size_t k, const InnerList& innerArray)
{
    if (k > innerArray_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = innerArray_arraysize + 1;
    InnerList *innerArray2 = new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        innerArray2[i] = this->innerArray[i];
    innerArray2[k] = innerArray;
    for (i = k + 1; i < newSize; i++)
        innerArray2[i] = this->innerArray[i-1];
    for (size_t i = 0; i < innerArray_arraysize; i++)
        drop(&this->innerArray[i]);
    delete [] this->innerArray;
    this->innerArray = innerArray2;
    innerArray_arraysize = newSize;
    for (size_t i = 0; i < innerArray_arraysize; i++)
        take(&this->innerArray[i]);
}

void TwoDInnerList::insertInnerArray(const InnerList& innerArray)
{
    insertInnerArray(innerArray_arraysize, innerArray);
}

void TwoDInnerList::eraseInnerArray(size_t k)
{
    if (k >= innerArray_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = innerArray_arraysize - 1;
    InnerList *innerArray2 = (newSize == 0) ? nullptr : new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        innerArray2[i] = this->innerArray[i];
    for (i = k; i < newSize; i++)
        innerArray2[i] = this->innerArray[i+1];
    for (size_t i = 0; i < innerArray_arraysize; i++)
        drop(&this->innerArray[i]);
    delete [] this->innerArray;
    this->innerArray = innerArray2;
    innerArray_arraysize = newSize;
    for (size_t i = 0; i < innerArray_arraysize; i++)
        take(&this->innerArray[i]);
}

class TwoDInnerListDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
    enum FieldConstants {
        FIELD_innerArray,
    };
  public:
    TwoDInnerListDescriptor();
    virtual ~TwoDInnerListDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(TwoDInnerListDescriptor)

TwoDInnerListDescriptor::TwoDInnerListDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::TwoDInnerList)), "omnetpp::cMessage")
{
    propertynames = nullptr;
}

TwoDInnerListDescriptor::~TwoDInnerListDescriptor()
{
    delete[] propertynames;
}

bool TwoDInnerListDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<TwoDInnerList *>(obj)!=nullptr;
}

const char **TwoDInnerListDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *TwoDInnerListDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int TwoDInnerListDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 1+basedesc->getFieldCount() : 1;
}

unsigned int TwoDInnerListDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_innerArray
    };
    return (field >= 0 && field < 1) ? fieldTypeFlags[field] : 0;
}

const char *TwoDInnerListDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "innerArray",
    };
    return (field >= 0 && field < 1) ? fieldNames[field] : nullptr;
}

int TwoDInnerListDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0] == 'i' && strcmp(fieldName, "innerArray") == 0) return base+0;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *TwoDInnerListDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "inet::InnerList",    // FIELD_innerArray
    };
    return (field >= 0 && field < 1) ? fieldTypeStrings[field] : nullptr;
}

const char **TwoDInnerListDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *TwoDInnerListDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int TwoDInnerListDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    TwoDInnerList *pp = (TwoDInnerList *)object; (void)pp;
    switch (field) {
        case FIELD_innerArray: return pp->getInnerArrayArraySize();
        default: return 0;
    }
}

const char *TwoDInnerListDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    TwoDInnerList *pp = (TwoDInnerList *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string TwoDInnerListDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    TwoDInnerList *pp = (TwoDInnerList *)object; (void)pp;
    switch (field) {
        case FIELD_innerArray: {std::stringstream out; out << pp->getInnerArray(i); return out.str();}
        default: return "";
    }
}

bool TwoDInnerListDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    TwoDInnerList *pp = (TwoDInnerList *)object; (void)pp;
    switch (field) {
        default: return false;
    }
}

const char *TwoDInnerListDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        case FIELD_innerArray: return omnetpp::opp_typename(typeid(InnerList));
        default: return nullptr;
    };
}

void *TwoDInnerListDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    TwoDInnerList *pp = (TwoDInnerList *)object; (void)pp;
    switch (field) {
        case FIELD_innerArray: return toVoidPtr(&pp->getInnerArray(i)); break;
        default: return nullptr;
    }
}

Register_Class(GenericAppMsg)

GenericAppMsg::GenericAppMsg() : ::inet::FieldsChunk()
{
}

GenericAppMsg::GenericAppMsg(const GenericAppMsg& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

GenericAppMsg::~GenericAppMsg()
{
    delete [] this->responsibility_list;
    delete [] this->responsibility_gpu_list;
    delete [] this->responsibility_flow_id_list;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    delete [] this->dst_idx_list;
    delete [] this->flow_ids_list;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    delete [] this->rsbf_seed_list;
    delete [] this->rsbf_bitstream_list;
}

GenericAppMsg& GenericAppMsg::operator=(const GenericAppMsg& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void GenericAppMsg::copy(const GenericAppMsg& other)
{
    this->expectedReplyLength = other.expectedReplyLength;
    this->replyDelay = other.replyDelay;
    this->serverClose = other.serverClose;
    this->requesterID = other.requesterID;
    this->is_micro_burst_flow = other.is_micro_burst_flow;
    this->query_id = other.query_id;
    this->requested_time = other.requested_time;
    this->total_flow_size = other.total_flow_size;
    delete [] this->responsibility_list;
    this->responsibility_list = (other.responsibility_list_arraysize==0) ? nullptr : new int[other.responsibility_list_arraysize];
    responsibility_list_arraysize = other.responsibility_list_arraysize;
    for (size_t i = 0; i < responsibility_list_arraysize; i++) {
        this->responsibility_list[i] = other.responsibility_list[i];
    }
    delete [] this->responsibility_gpu_list;
    this->responsibility_gpu_list = (other.responsibility_gpu_list_arraysize==0) ? nullptr : new int[other.responsibility_gpu_list_arraysize];
    responsibility_gpu_list_arraysize = other.responsibility_gpu_list_arraysize;
    for (size_t i = 0; i < responsibility_gpu_list_arraysize; i++) {
        this->responsibility_gpu_list[i] = other.responsibility_gpu_list[i];
    }
    delete [] this->responsibility_flow_id_list;
    this->responsibility_flow_id_list = (other.responsibility_flow_id_list_arraysize==0) ? nullptr : new unsigned long[other.responsibility_flow_id_list_arraysize];
    responsibility_flow_id_list_arraysize = other.responsibility_flow_id_list_arraysize;
    for (size_t i = 0; i < responsibility_flow_id_list_arraysize; i++) {
        this->responsibility_flow_id_list[i] = other.responsibility_flow_id_list[i];
    }
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    this->ports_to_dest_idx = (other.ports_to_dest_idx_arraysize==0) ? nullptr : new InnerList[other.ports_to_dest_idx_arraysize];
    ports_to_dest_idx_arraysize = other.ports_to_dest_idx_arraysize;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++) {
        this->ports_to_dest_idx[i] = other.ports_to_dest_idx[i];
        this->ports_to_dest_idx[i].setName(other.ports_to_dest_idx[i].getName());
        take(&this->ports_to_dest_idx[i]);
    }
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    this->jump_to_idx = (other.jump_to_idx_arraysize==0) ? nullptr : new InnerList[other.jump_to_idx_arraysize];
    jump_to_idx_arraysize = other.jump_to_idx_arraysize;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++) {
        this->jump_to_idx[i] = other.jump_to_idx[i];
        this->jump_to_idx[i].setName(other.jump_to_idx[i].getName());
        take(&this->jump_to_idx[i]);
    }
    delete [] this->dst_idx_list;
    this->dst_idx_list = (other.dst_idx_list_arraysize==0) ? nullptr : new int[other.dst_idx_list_arraysize];
    dst_idx_list_arraysize = other.dst_idx_list_arraysize;
    for (size_t i = 0; i < dst_idx_list_arraysize; i++) {
        this->dst_idx_list[i] = other.dst_idx_list[i];
    }
    delete [] this->flow_ids_list;
    this->flow_ids_list = (other.flow_ids_list_arraysize==0) ? nullptr : new unsigned long[other.flow_ids_list_arraysize];
    flow_ids_list_arraysize = other.flow_ids_list_arraysize;
    for (size_t i = 0; i < flow_ids_list_arraysize; i++) {
        this->flow_ids_list[i] = other.flow_ids_list[i];
    }
    this->app_name = other.app_name;
    this->app_full_path = other.app_full_path;
    this->src_gpu_idx = other.src_gpu_idx;
    this->dst_gpu_idx = other.dst_gpu_idx;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    this->all_ports_to_dst = (other.all_ports_to_dst_arraysize==0) ? nullptr : new TwoDInnerList[other.all_ports_to_dst_arraysize];
    all_ports_to_dst_arraysize = other.all_ports_to_dst_arraysize;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++) {
        this->all_ports_to_dst[i] = other.all_ports_to_dst[i];
        this->all_ports_to_dst[i].setName(other.all_ports_to_dst[i].getName());
        take(&this->all_ports_to_dst[i]);
    }
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    this->all_jump_to_idx = (other.all_jump_to_idx_arraysize==0) ? nullptr : new TwoDInnerList[other.all_jump_to_idx_arraysize];
    all_jump_to_idx_arraysize = other.all_jump_to_idx_arraysize;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++) {
        this->all_jump_to_idx[i] = other.all_jump_to_idx[i];
        this->all_jump_to_idx[i].setName(other.all_jump_to_idx[i].getName());
        take(&this->all_jump_to_idx[i]);
    }
    this->current_tree_idx = other.current_tree_idx;
    this->ring_end_server_idx = other.ring_end_server_idx;
    this->ring_end_gpu_idx = other.ring_end_gpu_idx;
    this->optireduce_in_reduction_phase = other.optireduce_in_reduction_phase;
    this->tree_search_type = other.tree_search_type;
    this->tree_traversal_dir = other.tree_traversal_dir;
    this->collective_type = other.collective_type;
    this->collective_alg_type = other.collective_alg_type;
    this->ina_leaf_aggregation_num = other.ina_leaf_aggregation_num;
    this->ina_spine_aggregation_num = other.ina_spine_aggregation_num;
    this->ina_core_aggregation_num = other.ina_core_aggregation_num;
    this->seq_num = other.seq_num;
    this->collective_scale = other.collective_scale;
    delete [] this->rsbf_seed_list;
    this->rsbf_seed_list = (other.rsbf_seed_list_arraysize==0) ? nullptr : new unsigned int[other.rsbf_seed_list_arraysize];
    rsbf_seed_list_arraysize = other.rsbf_seed_list_arraysize;
    for (size_t i = 0; i < rsbf_seed_list_arraysize; i++) {
        this->rsbf_seed_list[i] = other.rsbf_seed_list[i];
    }
    delete [] this->rsbf_bitstream_list;
    this->rsbf_bitstream_list = (other.rsbf_bitstream_list_arraysize==0) ? nullptr : new omnetpp::opp_string[other.rsbf_bitstream_list_arraysize];
    rsbf_bitstream_list_arraysize = other.rsbf_bitstream_list_arraysize;
    for (size_t i = 0; i < rsbf_bitstream_list_arraysize; i++) {
        this->rsbf_bitstream_list[i] = other.rsbf_bitstream_list[i];
    }
    this->rsbf_removed_bytes = other.rsbf_removed_bytes;
    this->init_tree_pkt_idx = other.init_tree_pkt_idx;
    this->dst_tor_idx = other.dst_tor_idx;
    this->partial_mcast_original_flow_size_bytes = other.partial_mcast_original_flow_size_bytes;
    this->controller_setup_finish_time = other.controller_setup_finish_time;
    this->num_msgs_in_tor_group = other.num_msgs_in_tor_group;
    this->tor_group_idx = other.tor_group_idx;
    this->agg_cidr_group_id = other.agg_cidr_group_id;
    this->agg_cidr_group_member_count = other.agg_cidr_group_member_count;
    this->core_cidr_group_id = other.core_cidr_group_id;
    this->core_cidr_group_member_count = other.core_cidr_group_member_count;
    this->in_src_sharding = other.in_src_sharding;
    this->using_orca = other.using_orca;
    this->using_elmo = other.using_elmo;
    this->useful_data_bytes = other.useful_data_bytes;
    this->elmo_overhead_pop_bytes = other.elmo_overhead_pop_bytes;
}

void GenericAppMsg::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->expectedReplyLength);
    doParsimPacking(b,this->replyDelay);
    doParsimPacking(b,this->serverClose);
    doParsimPacking(b,this->requesterID);
    doParsimPacking(b,this->is_micro_burst_flow);
    doParsimPacking(b,this->query_id);
    doParsimPacking(b,this->requested_time);
    doParsimPacking(b,this->total_flow_size);
    b->pack(responsibility_list_arraysize);
    doParsimArrayPacking(b,this->responsibility_list,responsibility_list_arraysize);
    b->pack(responsibility_gpu_list_arraysize);
    doParsimArrayPacking(b,this->responsibility_gpu_list,responsibility_gpu_list_arraysize);
    b->pack(responsibility_flow_id_list_arraysize);
    doParsimArrayPacking(b,this->responsibility_flow_id_list,responsibility_flow_id_list_arraysize);
    b->pack(ports_to_dest_idx_arraysize);
    doParsimArrayPacking(b,this->ports_to_dest_idx,ports_to_dest_idx_arraysize);
    b->pack(jump_to_idx_arraysize);
    doParsimArrayPacking(b,this->jump_to_idx,jump_to_idx_arraysize);
    b->pack(dst_idx_list_arraysize);
    doParsimArrayPacking(b,this->dst_idx_list,dst_idx_list_arraysize);
    b->pack(flow_ids_list_arraysize);
    doParsimArrayPacking(b,this->flow_ids_list,flow_ids_list_arraysize);
    doParsimPacking(b,this->app_name);
    doParsimPacking(b,this->app_full_path);
    doParsimPacking(b,this->src_gpu_idx);
    doParsimPacking(b,this->dst_gpu_idx);
    b->pack(all_ports_to_dst_arraysize);
    doParsimArrayPacking(b,this->all_ports_to_dst,all_ports_to_dst_arraysize);
    b->pack(all_jump_to_idx_arraysize);
    doParsimArrayPacking(b,this->all_jump_to_idx,all_jump_to_idx_arraysize);
    doParsimPacking(b,this->current_tree_idx);
    doParsimPacking(b,this->ring_end_server_idx);
    doParsimPacking(b,this->ring_end_gpu_idx);
    doParsimPacking(b,this->optireduce_in_reduction_phase);
    doParsimPacking(b,this->tree_search_type);
    doParsimPacking(b,this->tree_traversal_dir);
    doParsimPacking(b,this->collective_type);
    doParsimPacking(b,this->collective_alg_type);
    doParsimPacking(b,this->ina_leaf_aggregation_num);
    doParsimPacking(b,this->ina_spine_aggregation_num);
    doParsimPacking(b,this->ina_core_aggregation_num);
    doParsimPacking(b,this->seq_num);
    doParsimPacking(b,this->collective_scale);
    b->pack(rsbf_seed_list_arraysize);
    doParsimArrayPacking(b,this->rsbf_seed_list,rsbf_seed_list_arraysize);
    b->pack(rsbf_bitstream_list_arraysize);
    doParsimArrayPacking(b,this->rsbf_bitstream_list,rsbf_bitstream_list_arraysize);
    doParsimPacking(b,this->rsbf_removed_bytes);
    doParsimPacking(b,this->init_tree_pkt_idx);
    doParsimPacking(b,this->dst_tor_idx);
    doParsimPacking(b,this->partial_mcast_original_flow_size_bytes);
    doParsimPacking(b,this->controller_setup_finish_time);
    doParsimPacking(b,this->num_msgs_in_tor_group);
    doParsimPacking(b,this->tor_group_idx);
    doParsimPacking(b,this->agg_cidr_group_id);
    doParsimPacking(b,this->agg_cidr_group_member_count);
    doParsimPacking(b,this->core_cidr_group_id);
    doParsimPacking(b,this->core_cidr_group_member_count);
    doParsimPacking(b,this->in_src_sharding);
    doParsimPacking(b,this->using_orca);
    doParsimPacking(b,this->using_elmo);
    doParsimPacking(b,this->useful_data_bytes);
    doParsimPacking(b,this->elmo_overhead_pop_bytes);
}

void GenericAppMsg::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->expectedReplyLength);
    doParsimUnpacking(b,this->replyDelay);
    doParsimUnpacking(b,this->serverClose);
    doParsimUnpacking(b,this->requesterID);
    doParsimUnpacking(b,this->is_micro_burst_flow);
    doParsimUnpacking(b,this->query_id);
    doParsimUnpacking(b,this->requested_time);
    doParsimUnpacking(b,this->total_flow_size);
    delete [] this->responsibility_list;
    b->unpack(responsibility_list_arraysize);
    if (responsibility_list_arraysize == 0) {
        this->responsibility_list = nullptr;
    } else {
        this->responsibility_list = new int[responsibility_list_arraysize];
        doParsimArrayUnpacking(b,this->responsibility_list,responsibility_list_arraysize);
    }
    delete [] this->responsibility_gpu_list;
    b->unpack(responsibility_gpu_list_arraysize);
    if (responsibility_gpu_list_arraysize == 0) {
        this->responsibility_gpu_list = nullptr;
    } else {
        this->responsibility_gpu_list = new int[responsibility_gpu_list_arraysize];
        doParsimArrayUnpacking(b,this->responsibility_gpu_list,responsibility_gpu_list_arraysize);
    }
    delete [] this->responsibility_flow_id_list;
    b->unpack(responsibility_flow_id_list_arraysize);
    if (responsibility_flow_id_list_arraysize == 0) {
        this->responsibility_flow_id_list = nullptr;
    } else {
        this->responsibility_flow_id_list = new unsigned long[responsibility_flow_id_list_arraysize];
        doParsimArrayUnpacking(b,this->responsibility_flow_id_list,responsibility_flow_id_list_arraysize);
    }
    delete [] this->ports_to_dest_idx;
    b->unpack(ports_to_dest_idx_arraysize);
    if (ports_to_dest_idx_arraysize == 0) {
        this->ports_to_dest_idx = nullptr;
    } else {
        this->ports_to_dest_idx = new InnerList[ports_to_dest_idx_arraysize];
        doParsimArrayUnpacking(b,this->ports_to_dest_idx,ports_to_dest_idx_arraysize);
    }
    delete [] this->jump_to_idx;
    b->unpack(jump_to_idx_arraysize);
    if (jump_to_idx_arraysize == 0) {
        this->jump_to_idx = nullptr;
    } else {
        this->jump_to_idx = new InnerList[jump_to_idx_arraysize];
        doParsimArrayUnpacking(b,this->jump_to_idx,jump_to_idx_arraysize);
    }
    delete [] this->dst_idx_list;
    b->unpack(dst_idx_list_arraysize);
    if (dst_idx_list_arraysize == 0) {
        this->dst_idx_list = nullptr;
    } else {
        this->dst_idx_list = new int[dst_idx_list_arraysize];
        doParsimArrayUnpacking(b,this->dst_idx_list,dst_idx_list_arraysize);
    }
    delete [] this->flow_ids_list;
    b->unpack(flow_ids_list_arraysize);
    if (flow_ids_list_arraysize == 0) {
        this->flow_ids_list = nullptr;
    } else {
        this->flow_ids_list = new unsigned long[flow_ids_list_arraysize];
        doParsimArrayUnpacking(b,this->flow_ids_list,flow_ids_list_arraysize);
    }
    doParsimUnpacking(b,this->app_name);
    doParsimUnpacking(b,this->app_full_path);
    doParsimUnpacking(b,this->src_gpu_idx);
    doParsimUnpacking(b,this->dst_gpu_idx);
    delete [] this->all_ports_to_dst;
    b->unpack(all_ports_to_dst_arraysize);
    if (all_ports_to_dst_arraysize == 0) {
        this->all_ports_to_dst = nullptr;
    } else {
        this->all_ports_to_dst = new TwoDInnerList[all_ports_to_dst_arraysize];
        doParsimArrayUnpacking(b,this->all_ports_to_dst,all_ports_to_dst_arraysize);
    }
    delete [] this->all_jump_to_idx;
    b->unpack(all_jump_to_idx_arraysize);
    if (all_jump_to_idx_arraysize == 0) {
        this->all_jump_to_idx = nullptr;
    } else {
        this->all_jump_to_idx = new TwoDInnerList[all_jump_to_idx_arraysize];
        doParsimArrayUnpacking(b,this->all_jump_to_idx,all_jump_to_idx_arraysize);
    }
    doParsimUnpacking(b,this->current_tree_idx);
    doParsimUnpacking(b,this->ring_end_server_idx);
    doParsimUnpacking(b,this->ring_end_gpu_idx);
    doParsimUnpacking(b,this->optireduce_in_reduction_phase);
    doParsimUnpacking(b,this->tree_search_type);
    doParsimUnpacking(b,this->tree_traversal_dir);
    doParsimUnpacking(b,this->collective_type);
    doParsimUnpacking(b,this->collective_alg_type);
    doParsimUnpacking(b,this->ina_leaf_aggregation_num);
    doParsimUnpacking(b,this->ina_spine_aggregation_num);
    doParsimUnpacking(b,this->ina_core_aggregation_num);
    doParsimUnpacking(b,this->seq_num);
    doParsimUnpacking(b,this->collective_scale);
    delete [] this->rsbf_seed_list;
    b->unpack(rsbf_seed_list_arraysize);
    if (rsbf_seed_list_arraysize == 0) {
        this->rsbf_seed_list = nullptr;
    } else {
        this->rsbf_seed_list = new unsigned int[rsbf_seed_list_arraysize];
        doParsimArrayUnpacking(b,this->rsbf_seed_list,rsbf_seed_list_arraysize);
    }
    delete [] this->rsbf_bitstream_list;
    b->unpack(rsbf_bitstream_list_arraysize);
    if (rsbf_bitstream_list_arraysize == 0) {
        this->rsbf_bitstream_list = nullptr;
    } else {
        this->rsbf_bitstream_list = new omnetpp::opp_string[rsbf_bitstream_list_arraysize];
        doParsimArrayUnpacking(b,this->rsbf_bitstream_list,rsbf_bitstream_list_arraysize);
    }
    doParsimUnpacking(b,this->rsbf_removed_bytes);
    doParsimUnpacking(b,this->init_tree_pkt_idx);
    doParsimUnpacking(b,this->dst_tor_idx);
    doParsimUnpacking(b,this->partial_mcast_original_flow_size_bytes);
    doParsimUnpacking(b,this->controller_setup_finish_time);
    doParsimUnpacking(b,this->num_msgs_in_tor_group);
    doParsimUnpacking(b,this->tor_group_idx);
    doParsimUnpacking(b,this->agg_cidr_group_id);
    doParsimUnpacking(b,this->agg_cidr_group_member_count);
    doParsimUnpacking(b,this->core_cidr_group_id);
    doParsimUnpacking(b,this->core_cidr_group_member_count);
    doParsimUnpacking(b,this->in_src_sharding);
    doParsimUnpacking(b,this->using_orca);
    doParsimUnpacking(b,this->using_elmo);
    doParsimUnpacking(b,this->useful_data_bytes);
    doParsimUnpacking(b,this->elmo_overhead_pop_bytes);
}

B GenericAppMsg::getExpectedReplyLength() const
{
    return this->expectedReplyLength;
}

void GenericAppMsg::setExpectedReplyLength(B expectedReplyLength)
{
    handleChange();
    this->expectedReplyLength = expectedReplyLength;
}

double GenericAppMsg::getReplyDelay() const
{
    return this->replyDelay;
}

void GenericAppMsg::setReplyDelay(double replyDelay)
{
    handleChange();
    this->replyDelay = replyDelay;
}

bool GenericAppMsg::getServerClose() const
{
    return this->serverClose;
}

void GenericAppMsg::setServerClose(bool serverClose)
{
    handleChange();
    this->serverClose = serverClose;
}

unsigned long GenericAppMsg::getRequesterID() const
{
    return this->requesterID;
}

void GenericAppMsg::setRequesterID(unsigned long requesterID)
{
    handleChange();
    this->requesterID = requesterID;
}

bool GenericAppMsg::getIs_micro_burst_flow() const
{
    return this->is_micro_burst_flow;
}

void GenericAppMsg::setIs_micro_burst_flow(bool is_micro_burst_flow)
{
    handleChange();
    this->is_micro_burst_flow = is_micro_burst_flow;
}

unsigned long GenericAppMsg::getQuery_id() const
{
    return this->query_id;
}

void GenericAppMsg::setQuery_id(unsigned long query_id)
{
    handleChange();
    this->query_id = query_id;
}

omnetpp::simtime_t GenericAppMsg::getRequested_time() const
{
    return this->requested_time;
}

void GenericAppMsg::setRequested_time(omnetpp::simtime_t requested_time)
{
    handleChange();
    this->requested_time = requested_time;
}

b GenericAppMsg::getTotal_flow_size() const
{
    return this->total_flow_size;
}

void GenericAppMsg::setTotal_flow_size(b total_flow_size)
{
    handleChange();
    this->total_flow_size = total_flow_size;
}

size_t GenericAppMsg::getResponsibility_listArraySize() const
{
    return responsibility_list_arraysize;
}

int GenericAppMsg::getResponsibility_list(size_t k) const
{
    if (k >= responsibility_list_arraysize) throw omnetpp::cRuntimeError("Array of size responsibility_list_arraysize indexed by %lu", (unsigned long)k);
    return this->responsibility_list[k];
}

void GenericAppMsg::setResponsibility_listArraySize(size_t newSize)
{
    handleChange();
    int *responsibility_list2 = (newSize==0) ? nullptr : new int[newSize];
    size_t minSize = responsibility_list_arraysize < newSize ? responsibility_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        responsibility_list2[i] = this->responsibility_list[i];
    for (size_t i = minSize; i < newSize; i++)
        responsibility_list2[i] = 0;
    delete [] this->responsibility_list;
    this->responsibility_list = responsibility_list2;
    responsibility_list_arraysize = newSize;
}

void GenericAppMsg::setResponsibility_list(size_t k, int responsibility_list)
{
    if (k >= responsibility_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->responsibility_list[k] = responsibility_list;
}

void GenericAppMsg::insertResponsibility_list(size_t k, int responsibility_list)
{
    handleChange();
    if (k > responsibility_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = responsibility_list_arraysize + 1;
    int *responsibility_list2 = new int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        responsibility_list2[i] = this->responsibility_list[i];
    responsibility_list2[k] = responsibility_list;
    for (i = k + 1; i < newSize; i++)
        responsibility_list2[i] = this->responsibility_list[i-1];
    delete [] this->responsibility_list;
    this->responsibility_list = responsibility_list2;
    responsibility_list_arraysize = newSize;
}

void GenericAppMsg::insertResponsibility_list(int responsibility_list)
{
    insertResponsibility_list(responsibility_list_arraysize, responsibility_list);
}

void GenericAppMsg::eraseResponsibility_list(size_t k)
{
    if (k >= responsibility_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = responsibility_list_arraysize - 1;
    int *responsibility_list2 = (newSize == 0) ? nullptr : new int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        responsibility_list2[i] = this->responsibility_list[i];
    for (i = k; i < newSize; i++)
        responsibility_list2[i] = this->responsibility_list[i+1];
    delete [] this->responsibility_list;
    this->responsibility_list = responsibility_list2;
    responsibility_list_arraysize = newSize;
}

size_t GenericAppMsg::getResponsibility_gpu_listArraySize() const
{
    return responsibility_gpu_list_arraysize;
}

int GenericAppMsg::getResponsibility_gpu_list(size_t k) const
{
    if (k >= responsibility_gpu_list_arraysize) throw omnetpp::cRuntimeError("Array of size responsibility_gpu_list_arraysize indexed by %lu", (unsigned long)k);
    return this->responsibility_gpu_list[k];
}

void GenericAppMsg::setResponsibility_gpu_listArraySize(size_t newSize)
{
    handleChange();
    int *responsibility_gpu_list2 = (newSize==0) ? nullptr : new int[newSize];
    size_t minSize = responsibility_gpu_list_arraysize < newSize ? responsibility_gpu_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        responsibility_gpu_list2[i] = this->responsibility_gpu_list[i];
    for (size_t i = minSize; i < newSize; i++)
        responsibility_gpu_list2[i] = 0;
    delete [] this->responsibility_gpu_list;
    this->responsibility_gpu_list = responsibility_gpu_list2;
    responsibility_gpu_list_arraysize = newSize;
}

void GenericAppMsg::setResponsibility_gpu_list(size_t k, int responsibility_gpu_list)
{
    if (k >= responsibility_gpu_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->responsibility_gpu_list[k] = responsibility_gpu_list;
}

void GenericAppMsg::insertResponsibility_gpu_list(size_t k, int responsibility_gpu_list)
{
    handleChange();
    if (k > responsibility_gpu_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = responsibility_gpu_list_arraysize + 1;
    int *responsibility_gpu_list2 = new int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        responsibility_gpu_list2[i] = this->responsibility_gpu_list[i];
    responsibility_gpu_list2[k] = responsibility_gpu_list;
    for (i = k + 1; i < newSize; i++)
        responsibility_gpu_list2[i] = this->responsibility_gpu_list[i-1];
    delete [] this->responsibility_gpu_list;
    this->responsibility_gpu_list = responsibility_gpu_list2;
    responsibility_gpu_list_arraysize = newSize;
}

void GenericAppMsg::insertResponsibility_gpu_list(int responsibility_gpu_list)
{
    insertResponsibility_gpu_list(responsibility_gpu_list_arraysize, responsibility_gpu_list);
}

void GenericAppMsg::eraseResponsibility_gpu_list(size_t k)
{
    if (k >= responsibility_gpu_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = responsibility_gpu_list_arraysize - 1;
    int *responsibility_gpu_list2 = (newSize == 0) ? nullptr : new int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        responsibility_gpu_list2[i] = this->responsibility_gpu_list[i];
    for (i = k; i < newSize; i++)
        responsibility_gpu_list2[i] = this->responsibility_gpu_list[i+1];
    delete [] this->responsibility_gpu_list;
    this->responsibility_gpu_list = responsibility_gpu_list2;
    responsibility_gpu_list_arraysize = newSize;
}

size_t GenericAppMsg::getResponsibility_flow_id_listArraySize() const
{
    return responsibility_flow_id_list_arraysize;
}

unsigned long GenericAppMsg::getResponsibility_flow_id_list(size_t k) const
{
    if (k >= responsibility_flow_id_list_arraysize) throw omnetpp::cRuntimeError("Array of size responsibility_flow_id_list_arraysize indexed by %lu", (unsigned long)k);
    return this->responsibility_flow_id_list[k];
}

void GenericAppMsg::setResponsibility_flow_id_listArraySize(size_t newSize)
{
    handleChange();
    unsigned long *responsibility_flow_id_list2 = (newSize==0) ? nullptr : new unsigned long[newSize];
    size_t minSize = responsibility_flow_id_list_arraysize < newSize ? responsibility_flow_id_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        responsibility_flow_id_list2[i] = this->responsibility_flow_id_list[i];
    for (size_t i = minSize; i < newSize; i++)
        responsibility_flow_id_list2[i] = 0;
    delete [] this->responsibility_flow_id_list;
    this->responsibility_flow_id_list = responsibility_flow_id_list2;
    responsibility_flow_id_list_arraysize = newSize;
}

void GenericAppMsg::setResponsibility_flow_id_list(size_t k, unsigned long responsibility_flow_id_list)
{
    if (k >= responsibility_flow_id_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->responsibility_flow_id_list[k] = responsibility_flow_id_list;
}

void GenericAppMsg::insertResponsibility_flow_id_list(size_t k, unsigned long responsibility_flow_id_list)
{
    handleChange();
    if (k > responsibility_flow_id_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = responsibility_flow_id_list_arraysize + 1;
    unsigned long *responsibility_flow_id_list2 = new unsigned long[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        responsibility_flow_id_list2[i] = this->responsibility_flow_id_list[i];
    responsibility_flow_id_list2[k] = responsibility_flow_id_list;
    for (i = k + 1; i < newSize; i++)
        responsibility_flow_id_list2[i] = this->responsibility_flow_id_list[i-1];
    delete [] this->responsibility_flow_id_list;
    this->responsibility_flow_id_list = responsibility_flow_id_list2;
    responsibility_flow_id_list_arraysize = newSize;
}

void GenericAppMsg::insertResponsibility_flow_id_list(unsigned long responsibility_flow_id_list)
{
    insertResponsibility_flow_id_list(responsibility_flow_id_list_arraysize, responsibility_flow_id_list);
}

void GenericAppMsg::eraseResponsibility_flow_id_list(size_t k)
{
    if (k >= responsibility_flow_id_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = responsibility_flow_id_list_arraysize - 1;
    unsigned long *responsibility_flow_id_list2 = (newSize == 0) ? nullptr : new unsigned long[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        responsibility_flow_id_list2[i] = this->responsibility_flow_id_list[i];
    for (i = k; i < newSize; i++)
        responsibility_flow_id_list2[i] = this->responsibility_flow_id_list[i+1];
    delete [] this->responsibility_flow_id_list;
    this->responsibility_flow_id_list = responsibility_flow_id_list2;
    responsibility_flow_id_list_arraysize = newSize;
}

size_t GenericAppMsg::getPorts_to_dest_idxArraySize() const
{
    return ports_to_dest_idx_arraysize;
}

const InnerList& GenericAppMsg::getPorts_to_dest_idx(size_t k) const
{
    if (k >= ports_to_dest_idx_arraysize) throw omnetpp::cRuntimeError("Array of size ports_to_dest_idx_arraysize indexed by %lu", (unsigned long)k);
    return this->ports_to_dest_idx[k];
}

void GenericAppMsg::setPorts_to_dest_idxArraySize(size_t newSize)
{
    handleChange();
    InnerList *ports_to_dest_idx2 = (newSize==0) ? nullptr : new InnerList[newSize];
    size_t minSize = ports_to_dest_idx_arraysize < newSize ? ports_to_dest_idx_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i];
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    this->ports_to_dest_idx = ports_to_dest_idx2;
    ports_to_dest_idx_arraysize = newSize;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        take(&this->ports_to_dest_idx[i]);
}

void GenericAppMsg::setPorts_to_dest_idx(size_t k, const InnerList& ports_to_dest_idx)
{
    if (k >= ports_to_dest_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->ports_to_dest_idx[k] = ports_to_dest_idx;
}

void GenericAppMsg::insertPorts_to_dest_idx(size_t k, const InnerList& ports_to_dest_idx)
{
    handleChange();
    if (k > ports_to_dest_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = ports_to_dest_idx_arraysize + 1;
    InnerList *ports_to_dest_idx2 = new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i];
    ports_to_dest_idx2[k] = ports_to_dest_idx;
    for (i = k + 1; i < newSize; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i-1];
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    this->ports_to_dest_idx = ports_to_dest_idx2;
    ports_to_dest_idx_arraysize = newSize;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        take(&this->ports_to_dest_idx[i]);
}

void GenericAppMsg::insertPorts_to_dest_idx(const InnerList& ports_to_dest_idx)
{
    insertPorts_to_dest_idx(ports_to_dest_idx_arraysize, ports_to_dest_idx);
}

void GenericAppMsg::erasePorts_to_dest_idx(size_t k)
{
    if (k >= ports_to_dest_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = ports_to_dest_idx_arraysize - 1;
    InnerList *ports_to_dest_idx2 = (newSize == 0) ? nullptr : new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i];
    for (i = k; i < newSize; i++)
        ports_to_dest_idx2[i] = this->ports_to_dest_idx[i+1];
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        drop(&this->ports_to_dest_idx[i]);
    delete [] this->ports_to_dest_idx;
    this->ports_to_dest_idx = ports_to_dest_idx2;
    ports_to_dest_idx_arraysize = newSize;
    for (size_t i = 0; i < ports_to_dest_idx_arraysize; i++)
        take(&this->ports_to_dest_idx[i]);
}

size_t GenericAppMsg::getJump_to_idxArraySize() const
{
    return jump_to_idx_arraysize;
}

const InnerList& GenericAppMsg::getJump_to_idx(size_t k) const
{
    if (k >= jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size jump_to_idx_arraysize indexed by %lu", (unsigned long)k);
    return this->jump_to_idx[k];
}

void GenericAppMsg::setJump_to_idxArraySize(size_t newSize)
{
    handleChange();
    InnerList *jump_to_idx2 = (newSize==0) ? nullptr : new InnerList[newSize];
    size_t minSize = jump_to_idx_arraysize < newSize ? jump_to_idx_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        jump_to_idx2[i] = this->jump_to_idx[i];
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    this->jump_to_idx = jump_to_idx2;
    jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        take(&this->jump_to_idx[i]);
}

void GenericAppMsg::setJump_to_idx(size_t k, const InnerList& jump_to_idx)
{
    if (k >= jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->jump_to_idx[k] = jump_to_idx;
}

void GenericAppMsg::insertJump_to_idx(size_t k, const InnerList& jump_to_idx)
{
    handleChange();
    if (k > jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = jump_to_idx_arraysize + 1;
    InnerList *jump_to_idx2 = new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        jump_to_idx2[i] = this->jump_to_idx[i];
    jump_to_idx2[k] = jump_to_idx;
    for (i = k + 1; i < newSize; i++)
        jump_to_idx2[i] = this->jump_to_idx[i-1];
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    this->jump_to_idx = jump_to_idx2;
    jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        take(&this->jump_to_idx[i]);
}

void GenericAppMsg::insertJump_to_idx(const InnerList& jump_to_idx)
{
    insertJump_to_idx(jump_to_idx_arraysize, jump_to_idx);
}

void GenericAppMsg::eraseJump_to_idx(size_t k)
{
    if (k >= jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = jump_to_idx_arraysize - 1;
    InnerList *jump_to_idx2 = (newSize == 0) ? nullptr : new InnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        jump_to_idx2[i] = this->jump_to_idx[i];
    for (i = k; i < newSize; i++)
        jump_to_idx2[i] = this->jump_to_idx[i+1];
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        drop(&this->jump_to_idx[i]);
    delete [] this->jump_to_idx;
    this->jump_to_idx = jump_to_idx2;
    jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < jump_to_idx_arraysize; i++)
        take(&this->jump_to_idx[i]);
}

size_t GenericAppMsg::getDst_idx_listArraySize() const
{
    return dst_idx_list_arraysize;
}

int GenericAppMsg::getDst_idx_list(size_t k) const
{
    if (k >= dst_idx_list_arraysize) throw omnetpp::cRuntimeError("Array of size dst_idx_list_arraysize indexed by %lu", (unsigned long)k);
    return this->dst_idx_list[k];
}

void GenericAppMsg::setDst_idx_listArraySize(size_t newSize)
{
    handleChange();
    int *dst_idx_list2 = (newSize==0) ? nullptr : new int[newSize];
    size_t minSize = dst_idx_list_arraysize < newSize ? dst_idx_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        dst_idx_list2[i] = this->dst_idx_list[i];
    for (size_t i = minSize; i < newSize; i++)
        dst_idx_list2[i] = 0;
    delete [] this->dst_idx_list;
    this->dst_idx_list = dst_idx_list2;
    dst_idx_list_arraysize = newSize;
}

void GenericAppMsg::setDst_idx_list(size_t k, int dst_idx_list)
{
    if (k >= dst_idx_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->dst_idx_list[k] = dst_idx_list;
}

void GenericAppMsg::insertDst_idx_list(size_t k, int dst_idx_list)
{
    handleChange();
    if (k > dst_idx_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = dst_idx_list_arraysize + 1;
    int *dst_idx_list2 = new int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        dst_idx_list2[i] = this->dst_idx_list[i];
    dst_idx_list2[k] = dst_idx_list;
    for (i = k + 1; i < newSize; i++)
        dst_idx_list2[i] = this->dst_idx_list[i-1];
    delete [] this->dst_idx_list;
    this->dst_idx_list = dst_idx_list2;
    dst_idx_list_arraysize = newSize;
}

void GenericAppMsg::insertDst_idx_list(int dst_idx_list)
{
    insertDst_idx_list(dst_idx_list_arraysize, dst_idx_list);
}

void GenericAppMsg::eraseDst_idx_list(size_t k)
{
    if (k >= dst_idx_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = dst_idx_list_arraysize - 1;
    int *dst_idx_list2 = (newSize == 0) ? nullptr : new int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        dst_idx_list2[i] = this->dst_idx_list[i];
    for (i = k; i < newSize; i++)
        dst_idx_list2[i] = this->dst_idx_list[i+1];
    delete [] this->dst_idx_list;
    this->dst_idx_list = dst_idx_list2;
    dst_idx_list_arraysize = newSize;
}

size_t GenericAppMsg::getFlow_ids_listArraySize() const
{
    return flow_ids_list_arraysize;
}

unsigned long GenericAppMsg::getFlow_ids_list(size_t k) const
{
    if (k >= flow_ids_list_arraysize) throw omnetpp::cRuntimeError("Array of size flow_ids_list_arraysize indexed by %lu", (unsigned long)k);
    return this->flow_ids_list[k];
}

void GenericAppMsg::setFlow_ids_listArraySize(size_t newSize)
{
    handleChange();
    unsigned long *flow_ids_list2 = (newSize==0) ? nullptr : new unsigned long[newSize];
    size_t minSize = flow_ids_list_arraysize < newSize ? flow_ids_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        flow_ids_list2[i] = this->flow_ids_list[i];
    for (size_t i = minSize; i < newSize; i++)
        flow_ids_list2[i] = 0;
    delete [] this->flow_ids_list;
    this->flow_ids_list = flow_ids_list2;
    flow_ids_list_arraysize = newSize;
}

void GenericAppMsg::setFlow_ids_list(size_t k, unsigned long flow_ids_list)
{
    if (k >= flow_ids_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->flow_ids_list[k] = flow_ids_list;
}

void GenericAppMsg::insertFlow_ids_list(size_t k, unsigned long flow_ids_list)
{
    handleChange();
    if (k > flow_ids_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = flow_ids_list_arraysize + 1;
    unsigned long *flow_ids_list2 = new unsigned long[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        flow_ids_list2[i] = this->flow_ids_list[i];
    flow_ids_list2[k] = flow_ids_list;
    for (i = k + 1; i < newSize; i++)
        flow_ids_list2[i] = this->flow_ids_list[i-1];
    delete [] this->flow_ids_list;
    this->flow_ids_list = flow_ids_list2;
    flow_ids_list_arraysize = newSize;
}

void GenericAppMsg::insertFlow_ids_list(unsigned long flow_ids_list)
{
    insertFlow_ids_list(flow_ids_list_arraysize, flow_ids_list);
}

void GenericAppMsg::eraseFlow_ids_list(size_t k)
{
    if (k >= flow_ids_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = flow_ids_list_arraysize - 1;
    unsigned long *flow_ids_list2 = (newSize == 0) ? nullptr : new unsigned long[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        flow_ids_list2[i] = this->flow_ids_list[i];
    for (i = k; i < newSize; i++)
        flow_ids_list2[i] = this->flow_ids_list[i+1];
    delete [] this->flow_ids_list;
    this->flow_ids_list = flow_ids_list2;
    flow_ids_list_arraysize = newSize;
}

const char * GenericAppMsg::getApp_name() const
{
    return this->app_name.c_str();
}

void GenericAppMsg::setApp_name(const char * app_name)
{
    handleChange();
    this->app_name = app_name;
}

const char * GenericAppMsg::getApp_full_path() const
{
    return this->app_full_path.c_str();
}

void GenericAppMsg::setApp_full_path(const char * app_full_path)
{
    handleChange();
    this->app_full_path = app_full_path;
}

int GenericAppMsg::getSrc_gpu_idx() const
{
    return this->src_gpu_idx;
}

void GenericAppMsg::setSrc_gpu_idx(int src_gpu_idx)
{
    handleChange();
    this->src_gpu_idx = src_gpu_idx;
}

int GenericAppMsg::getDst_gpu_idx() const
{
    return this->dst_gpu_idx;
}

void GenericAppMsg::setDst_gpu_idx(int dst_gpu_idx)
{
    handleChange();
    this->dst_gpu_idx = dst_gpu_idx;
}

size_t GenericAppMsg::getAll_ports_to_dstArraySize() const
{
    return all_ports_to_dst_arraysize;
}

const TwoDInnerList& GenericAppMsg::getAll_ports_to_dst(size_t k) const
{
    if (k >= all_ports_to_dst_arraysize) throw omnetpp::cRuntimeError("Array of size all_ports_to_dst_arraysize indexed by %lu", (unsigned long)k);
    return this->all_ports_to_dst[k];
}

void GenericAppMsg::setAll_ports_to_dstArraySize(size_t newSize)
{
    handleChange();
    TwoDInnerList *all_ports_to_dst2 = (newSize==0) ? nullptr : new TwoDInnerList[newSize];
    size_t minSize = all_ports_to_dst_arraysize < newSize ? all_ports_to_dst_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i];
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    this->all_ports_to_dst = all_ports_to_dst2;
    all_ports_to_dst_arraysize = newSize;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        take(&this->all_ports_to_dst[i]);
}

void GenericAppMsg::setAll_ports_to_dst(size_t k, const TwoDInnerList& all_ports_to_dst)
{
    if (k >= all_ports_to_dst_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->all_ports_to_dst[k] = all_ports_to_dst;
}

void GenericAppMsg::insertAll_ports_to_dst(size_t k, const TwoDInnerList& all_ports_to_dst)
{
    handleChange();
    if (k > all_ports_to_dst_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = all_ports_to_dst_arraysize + 1;
    TwoDInnerList *all_ports_to_dst2 = new TwoDInnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i];
    all_ports_to_dst2[k] = all_ports_to_dst;
    for (i = k + 1; i < newSize; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i-1];
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    this->all_ports_to_dst = all_ports_to_dst2;
    all_ports_to_dst_arraysize = newSize;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        take(&this->all_ports_to_dst[i]);
}

void GenericAppMsg::insertAll_ports_to_dst(const TwoDInnerList& all_ports_to_dst)
{
    insertAll_ports_to_dst(all_ports_to_dst_arraysize, all_ports_to_dst);
}

void GenericAppMsg::eraseAll_ports_to_dst(size_t k)
{
    if (k >= all_ports_to_dst_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = all_ports_to_dst_arraysize - 1;
    TwoDInnerList *all_ports_to_dst2 = (newSize == 0) ? nullptr : new TwoDInnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i];
    for (i = k; i < newSize; i++)
        all_ports_to_dst2[i] = this->all_ports_to_dst[i+1];
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        drop(&this->all_ports_to_dst[i]);
    delete [] this->all_ports_to_dst;
    this->all_ports_to_dst = all_ports_to_dst2;
    all_ports_to_dst_arraysize = newSize;
    for (size_t i = 0; i < all_ports_to_dst_arraysize; i++)
        take(&this->all_ports_to_dst[i]);
}

size_t GenericAppMsg::getAll_jump_to_idxArraySize() const
{
    return all_jump_to_idx_arraysize;
}

const TwoDInnerList& GenericAppMsg::getAll_jump_to_idx(size_t k) const
{
    if (k >= all_jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size all_jump_to_idx_arraysize indexed by %lu", (unsigned long)k);
    return this->all_jump_to_idx[k];
}

void GenericAppMsg::setAll_jump_to_idxArraySize(size_t newSize)
{
    handleChange();
    TwoDInnerList *all_jump_to_idx2 = (newSize==0) ? nullptr : new TwoDInnerList[newSize];
    size_t minSize = all_jump_to_idx_arraysize < newSize ? all_jump_to_idx_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i];
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    this->all_jump_to_idx = all_jump_to_idx2;
    all_jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        take(&this->all_jump_to_idx[i]);
}

void GenericAppMsg::setAll_jump_to_idx(size_t k, const TwoDInnerList& all_jump_to_idx)
{
    if (k >= all_jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->all_jump_to_idx[k] = all_jump_to_idx;
}

void GenericAppMsg::insertAll_jump_to_idx(size_t k, const TwoDInnerList& all_jump_to_idx)
{
    handleChange();
    if (k > all_jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = all_jump_to_idx_arraysize + 1;
    TwoDInnerList *all_jump_to_idx2 = new TwoDInnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i];
    all_jump_to_idx2[k] = all_jump_to_idx;
    for (i = k + 1; i < newSize; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i-1];
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    this->all_jump_to_idx = all_jump_to_idx2;
    all_jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        take(&this->all_jump_to_idx[i]);
}

void GenericAppMsg::insertAll_jump_to_idx(const TwoDInnerList& all_jump_to_idx)
{
    insertAll_jump_to_idx(all_jump_to_idx_arraysize, all_jump_to_idx);
}

void GenericAppMsg::eraseAll_jump_to_idx(size_t k)
{
    if (k >= all_jump_to_idx_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = all_jump_to_idx_arraysize - 1;
    TwoDInnerList *all_jump_to_idx2 = (newSize == 0) ? nullptr : new TwoDInnerList[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i];
    for (i = k; i < newSize; i++)
        all_jump_to_idx2[i] = this->all_jump_to_idx[i+1];
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        drop(&this->all_jump_to_idx[i]);
    delete [] this->all_jump_to_idx;
    this->all_jump_to_idx = all_jump_to_idx2;
    all_jump_to_idx_arraysize = newSize;
    for (size_t i = 0; i < all_jump_to_idx_arraysize; i++)
        take(&this->all_jump_to_idx[i]);
}

int GenericAppMsg::getCurrent_tree_idx() const
{
    return this->current_tree_idx;
}

void GenericAppMsg::setCurrent_tree_idx(int current_tree_idx)
{
    handleChange();
    this->current_tree_idx = current_tree_idx;
}

int GenericAppMsg::getRing_end_server_idx() const
{
    return this->ring_end_server_idx;
}

void GenericAppMsg::setRing_end_server_idx(int ring_end_server_idx)
{
    handleChange();
    this->ring_end_server_idx = ring_end_server_idx;
}

int GenericAppMsg::getRing_end_gpu_idx() const
{
    return this->ring_end_gpu_idx;
}

void GenericAppMsg::setRing_end_gpu_idx(int ring_end_gpu_idx)
{
    handleChange();
    this->ring_end_gpu_idx = ring_end_gpu_idx;
}

bool GenericAppMsg::getOptireduce_in_reduction_phase() const
{
    return this->optireduce_in_reduction_phase;
}

void GenericAppMsg::setOptireduce_in_reduction_phase(bool optireduce_in_reduction_phase)
{
    handleChange();
    this->optireduce_in_reduction_phase = optireduce_in_reduction_phase;
}

int GenericAppMsg::getTree_search_type() const
{
    return this->tree_search_type;
}

void GenericAppMsg::setTree_search_type(int tree_search_type)
{
    handleChange();
    this->tree_search_type = tree_search_type;
}

int GenericAppMsg::getTree_traversal_dir() const
{
    return this->tree_traversal_dir;
}

void GenericAppMsg::setTree_traversal_dir(int tree_traversal_dir)
{
    handleChange();
    this->tree_traversal_dir = tree_traversal_dir;
}

int GenericAppMsg::getCollective_type() const
{
    return this->collective_type;
}

void GenericAppMsg::setCollective_type(int collective_type)
{
    handleChange();
    this->collective_type = collective_type;
}

int GenericAppMsg::getCollective_alg_type() const
{
    return this->collective_alg_type;
}

void GenericAppMsg::setCollective_alg_type(int collective_alg_type)
{
    handleChange();
    this->collective_alg_type = collective_alg_type;
}

int GenericAppMsg::getIna_leaf_aggregation_num() const
{
    return this->ina_leaf_aggregation_num;
}

void GenericAppMsg::setIna_leaf_aggregation_num(int ina_leaf_aggregation_num)
{
    handleChange();
    this->ina_leaf_aggregation_num = ina_leaf_aggregation_num;
}

int GenericAppMsg::getIna_spine_aggregation_num() const
{
    return this->ina_spine_aggregation_num;
}

void GenericAppMsg::setIna_spine_aggregation_num(int ina_spine_aggregation_num)
{
    handleChange();
    this->ina_spine_aggregation_num = ina_spine_aggregation_num;
}

int GenericAppMsg::getIna_core_aggregation_num() const
{
    return this->ina_core_aggregation_num;
}

void GenericAppMsg::setIna_core_aggregation_num(int ina_core_aggregation_num)
{
    handleChange();
    this->ina_core_aggregation_num = ina_core_aggregation_num;
}

unsigned long GenericAppMsg::getSeq_num() const
{
    return this->seq_num;
}

void GenericAppMsg::setSeq_num(unsigned long seq_num)
{
    handleChange();
    this->seq_num = seq_num;
}

unsigned int GenericAppMsg::getCollective_scale() const
{
    return this->collective_scale;
}

void GenericAppMsg::setCollective_scale(unsigned int collective_scale)
{
    handleChange();
    this->collective_scale = collective_scale;
}

size_t GenericAppMsg::getRsbf_seed_listArraySize() const
{
    return rsbf_seed_list_arraysize;
}

unsigned int GenericAppMsg::getRsbf_seed_list(size_t k) const
{
    if (k >= rsbf_seed_list_arraysize) throw omnetpp::cRuntimeError("Array of size rsbf_seed_list_arraysize indexed by %lu", (unsigned long)k);
    return this->rsbf_seed_list[k];
}

void GenericAppMsg::setRsbf_seed_listArraySize(size_t newSize)
{
    handleChange();
    unsigned int *rsbf_seed_list2 = (newSize==0) ? nullptr : new unsigned int[newSize];
    size_t minSize = rsbf_seed_list_arraysize < newSize ? rsbf_seed_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i];
    for (size_t i = minSize; i < newSize; i++)
        rsbf_seed_list2[i] = 0;
    delete [] this->rsbf_seed_list;
    this->rsbf_seed_list = rsbf_seed_list2;
    rsbf_seed_list_arraysize = newSize;
}

void GenericAppMsg::setRsbf_seed_list(size_t k, unsigned int rsbf_seed_list)
{
    if (k >= rsbf_seed_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->rsbf_seed_list[k] = rsbf_seed_list;
}

void GenericAppMsg::insertRsbf_seed_list(size_t k, unsigned int rsbf_seed_list)
{
    handleChange();
    if (k > rsbf_seed_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = rsbf_seed_list_arraysize + 1;
    unsigned int *rsbf_seed_list2 = new unsigned int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i];
    rsbf_seed_list2[k] = rsbf_seed_list;
    for (i = k + 1; i < newSize; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i-1];
    delete [] this->rsbf_seed_list;
    this->rsbf_seed_list = rsbf_seed_list2;
    rsbf_seed_list_arraysize = newSize;
}

void GenericAppMsg::insertRsbf_seed_list(unsigned int rsbf_seed_list)
{
    insertRsbf_seed_list(rsbf_seed_list_arraysize, rsbf_seed_list);
}

void GenericAppMsg::eraseRsbf_seed_list(size_t k)
{
    if (k >= rsbf_seed_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = rsbf_seed_list_arraysize - 1;
    unsigned int *rsbf_seed_list2 = (newSize == 0) ? nullptr : new unsigned int[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i];
    for (i = k; i < newSize; i++)
        rsbf_seed_list2[i] = this->rsbf_seed_list[i+1];
    delete [] this->rsbf_seed_list;
    this->rsbf_seed_list = rsbf_seed_list2;
    rsbf_seed_list_arraysize = newSize;
}

size_t GenericAppMsg::getRsbf_bitstream_listArraySize() const
{
    return rsbf_bitstream_list_arraysize;
}

const char * GenericAppMsg::getRsbf_bitstream_list(size_t k) const
{
    if (k >= rsbf_bitstream_list_arraysize) throw omnetpp::cRuntimeError("Array of size rsbf_bitstream_list_arraysize indexed by %lu", (unsigned long)k);
    return this->rsbf_bitstream_list[k].c_str();
}

void GenericAppMsg::setRsbf_bitstream_listArraySize(size_t newSize)
{
    handleChange();
    omnetpp::opp_string *rsbf_bitstream_list2 = (newSize==0) ? nullptr : new omnetpp::opp_string[newSize];
    size_t minSize = rsbf_bitstream_list_arraysize < newSize ? rsbf_bitstream_list_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i];
    delete [] this->rsbf_bitstream_list;
    this->rsbf_bitstream_list = rsbf_bitstream_list2;
    rsbf_bitstream_list_arraysize = newSize;
}

void GenericAppMsg::setRsbf_bitstream_list(size_t k, const char * rsbf_bitstream_list)
{
    if (k >= rsbf_bitstream_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    this->rsbf_bitstream_list[k] = rsbf_bitstream_list;
}

void GenericAppMsg::insertRsbf_bitstream_list(size_t k, const char * rsbf_bitstream_list)
{
    handleChange();
    if (k > rsbf_bitstream_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    size_t newSize = rsbf_bitstream_list_arraysize + 1;
    omnetpp::opp_string *rsbf_bitstream_list2 = new omnetpp::opp_string[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i];
    rsbf_bitstream_list2[k] = rsbf_bitstream_list;
    for (i = k + 1; i < newSize; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i-1];
    delete [] this->rsbf_bitstream_list;
    this->rsbf_bitstream_list = rsbf_bitstream_list2;
    rsbf_bitstream_list_arraysize = newSize;
}

void GenericAppMsg::insertRsbf_bitstream_list(const char * rsbf_bitstream_list)
{
    insertRsbf_bitstream_list(rsbf_bitstream_list_arraysize, rsbf_bitstream_list);
}

void GenericAppMsg::eraseRsbf_bitstream_list(size_t k)
{
    if (k >= rsbf_bitstream_list_arraysize) throw omnetpp::cRuntimeError("Array of size  indexed by %lu", (unsigned long)k);
    handleChange();
    size_t newSize = rsbf_bitstream_list_arraysize - 1;
    omnetpp::opp_string *rsbf_bitstream_list2 = (newSize == 0) ? nullptr : new omnetpp::opp_string[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i];
    for (i = k; i < newSize; i++)
        rsbf_bitstream_list2[i] = this->rsbf_bitstream_list[i+1];
    delete [] this->rsbf_bitstream_list;
    this->rsbf_bitstream_list = rsbf_bitstream_list2;
    rsbf_bitstream_list_arraysize = newSize;
}

unsigned long GenericAppMsg::getRsbf_removed_bytes() const
{
    return this->rsbf_removed_bytes;
}

void GenericAppMsg::setRsbf_removed_bytes(unsigned long rsbf_removed_bytes)
{
    handleChange();
    this->rsbf_removed_bytes = rsbf_removed_bytes;
}

int GenericAppMsg::getInit_tree_pkt_idx() const
{
    return this->init_tree_pkt_idx;
}

void GenericAppMsg::setInit_tree_pkt_idx(int init_tree_pkt_idx)
{
    handleChange();
    this->init_tree_pkt_idx = init_tree_pkt_idx;
}

int GenericAppMsg::getDst_tor_idx() const
{
    return this->dst_tor_idx;
}

void GenericAppMsg::setDst_tor_idx(int dst_tor_idx)
{
    handleChange();
    this->dst_tor_idx = dst_tor_idx;
}

unsigned long GenericAppMsg::getPartial_mcast_original_flow_size_bytes() const
{
    return this->partial_mcast_original_flow_size_bytes;
}

void GenericAppMsg::setPartial_mcast_original_flow_size_bytes(unsigned long partial_mcast_original_flow_size_bytes)
{
    handleChange();
    this->partial_mcast_original_flow_size_bytes = partial_mcast_original_flow_size_bytes;
}

omnetpp::simtime_t GenericAppMsg::getController_setup_finish_time() const
{
    return this->controller_setup_finish_time;
}

void GenericAppMsg::setController_setup_finish_time(omnetpp::simtime_t controller_setup_finish_time)
{
    handleChange();
    this->controller_setup_finish_time = controller_setup_finish_time;
}

int GenericAppMsg::getNum_msgs_in_tor_group() const
{
    return this->num_msgs_in_tor_group;
}

void GenericAppMsg::setNum_msgs_in_tor_group(int num_msgs_in_tor_group)
{
    handleChange();
    this->num_msgs_in_tor_group = num_msgs_in_tor_group;
}

int GenericAppMsg::getTor_group_idx() const
{
    return this->tor_group_idx;
}

void GenericAppMsg::setTor_group_idx(int tor_group_idx)
{
    handleChange();
    this->tor_group_idx = tor_group_idx;
}

const char * GenericAppMsg::getAgg_cidr_group_id() const
{
    return this->agg_cidr_group_id.c_str();
}

void GenericAppMsg::setAgg_cidr_group_id(const char * agg_cidr_group_id)
{
    handleChange();
    this->agg_cidr_group_id = agg_cidr_group_id;
}

unsigned long GenericAppMsg::getAgg_cidr_group_member_count() const
{
    return this->agg_cidr_group_member_count;
}

void GenericAppMsg::setAgg_cidr_group_member_count(unsigned long agg_cidr_group_member_count)
{
    handleChange();
    this->agg_cidr_group_member_count = agg_cidr_group_member_count;
}

const char * GenericAppMsg::getCore_cidr_group_id() const
{
    return this->core_cidr_group_id.c_str();
}

void GenericAppMsg::setCore_cidr_group_id(const char * core_cidr_group_id)
{
    handleChange();
    this->core_cidr_group_id = core_cidr_group_id;
}

unsigned long GenericAppMsg::getCore_cidr_group_member_count() const
{
    return this->core_cidr_group_member_count;
}

void GenericAppMsg::setCore_cidr_group_member_count(unsigned long core_cidr_group_member_count)
{
    handleChange();
    this->core_cidr_group_member_count = core_cidr_group_member_count;
}

bool GenericAppMsg::getIn_src_sharding() const
{
    return this->in_src_sharding;
}

void GenericAppMsg::setIn_src_sharding(bool in_src_sharding)
{
    handleChange();
    this->in_src_sharding = in_src_sharding;
}

bool GenericAppMsg::getUsing_orca() const
{
    return this->using_orca;
}

void GenericAppMsg::setUsing_orca(bool using_orca)
{
    handleChange();
    this->using_orca = using_orca;
}

bool GenericAppMsg::getUsing_elmo() const
{
    return this->using_elmo;
}

void GenericAppMsg::setUsing_elmo(bool using_elmo)
{
    handleChange();
    this->using_elmo = using_elmo;
}

int GenericAppMsg::getUseful_data_bytes() const
{
    return this->useful_data_bytes;
}

void GenericAppMsg::setUseful_data_bytes(int useful_data_bytes)
{
    handleChange();
    this->useful_data_bytes = useful_data_bytes;
}

int GenericAppMsg::getElmo_overhead_pop_bytes() const
{
    return this->elmo_overhead_pop_bytes;
}

void GenericAppMsg::setElmo_overhead_pop_bytes(int elmo_overhead_pop_bytes)
{
    handleChange();
    this->elmo_overhead_pop_bytes = elmo_overhead_pop_bytes;
}

class GenericAppMsgDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertynames;
    enum FieldConstants {
        FIELD_expectedReplyLength,
        FIELD_replyDelay,
        FIELD_serverClose,
        FIELD_requesterID,
        FIELD_is_micro_burst_flow,
        FIELD_query_id,
        FIELD_requested_time,
        FIELD_total_flow_size,
        FIELD_responsibility_list,
        FIELD_responsibility_gpu_list,
        FIELD_responsibility_flow_id_list,
        FIELD_ports_to_dest_idx,
        FIELD_jump_to_idx,
        FIELD_dst_idx_list,
        FIELD_flow_ids_list,
        FIELD_app_name,
        FIELD_app_full_path,
        FIELD_src_gpu_idx,
        FIELD_dst_gpu_idx,
        FIELD_all_ports_to_dst,
        FIELD_all_jump_to_idx,
        FIELD_current_tree_idx,
        FIELD_ring_end_server_idx,
        FIELD_ring_end_gpu_idx,
        FIELD_optireduce_in_reduction_phase,
        FIELD_tree_search_type,
        FIELD_tree_traversal_dir,
        FIELD_collective_type,
        FIELD_collective_alg_type,
        FIELD_ina_leaf_aggregation_num,
        FIELD_ina_spine_aggregation_num,
        FIELD_ina_core_aggregation_num,
        FIELD_seq_num,
        FIELD_collective_scale,
        FIELD_rsbf_seed_list,
        FIELD_rsbf_bitstream_list,
        FIELD_rsbf_removed_bytes,
        FIELD_init_tree_pkt_idx,
        FIELD_dst_tor_idx,
        FIELD_partial_mcast_original_flow_size_bytes,
        FIELD_controller_setup_finish_time,
        FIELD_num_msgs_in_tor_group,
        FIELD_tor_group_idx,
        FIELD_agg_cidr_group_id,
        FIELD_agg_cidr_group_member_count,
        FIELD_core_cidr_group_id,
        FIELD_core_cidr_group_member_count,
        FIELD_in_src_sharding,
        FIELD_using_orca,
        FIELD_using_elmo,
        FIELD_useful_data_bytes,
        FIELD_elmo_overhead_pop_bytes,
    };
  public:
    GenericAppMsgDescriptor();
    virtual ~GenericAppMsgDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyname) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyname) const override;
    virtual int getFieldArraySize(void *object, int field) const override;

    virtual const char *getFieldDynamicTypeString(void *object, int field, int i) const override;
    virtual std::string getFieldValueAsString(void *object, int field, int i) const override;
    virtual bool setFieldValueAsString(void *object, int field, int i, const char *value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual void *getFieldStructValuePointer(void *object, int field, int i) const override;
};

Register_ClassDescriptor(GenericAppMsgDescriptor)

GenericAppMsgDescriptor::GenericAppMsgDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::GenericAppMsg)), "inet::FieldsChunk")
{
    propertynames = nullptr;
}

GenericAppMsgDescriptor::~GenericAppMsgDescriptor()
{
    delete[] propertynames;
}

bool GenericAppMsgDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<GenericAppMsg *>(obj)!=nullptr;
}

const char **GenericAppMsgDescriptor::getPropertyNames() const
{
    if (!propertynames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
        const char **basenames = basedesc ? basedesc->getPropertyNames() : nullptr;
        propertynames = mergeLists(basenames, names);
    }
    return propertynames;
}

const char *GenericAppMsgDescriptor::getProperty(const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? basedesc->getProperty(propertyname) : nullptr;
}

int GenericAppMsgDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    return basedesc ? 52+basedesc->getFieldCount() : 52;
}

unsigned int GenericAppMsgDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeFlags(field);
        field -= basedesc->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_expectedReplyLength
        FD_ISEDITABLE,    // FIELD_replyDelay
        FD_ISEDITABLE,    // FIELD_serverClose
        FD_ISEDITABLE,    // FIELD_requesterID
        FD_ISEDITABLE,    // FIELD_is_micro_burst_flow
        FD_ISEDITABLE,    // FIELD_query_id
        0,    // FIELD_requested_time
        FD_ISEDITABLE,    // FIELD_total_flow_size
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_responsibility_list
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_responsibility_gpu_list
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_responsibility_flow_id_list
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_ports_to_dest_idx
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_jump_to_idx
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_dst_idx_list
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_flow_ids_list
        FD_ISEDITABLE,    // FIELD_app_name
        FD_ISEDITABLE,    // FIELD_app_full_path
        FD_ISEDITABLE,    // FIELD_src_gpu_idx
        FD_ISEDITABLE,    // FIELD_dst_gpu_idx
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_all_ports_to_dst
        FD_ISARRAY | FD_ISCOMPOUND | FD_ISCOBJECT | FD_ISCOWNEDOBJECT,    // FIELD_all_jump_to_idx
        FD_ISEDITABLE,    // FIELD_current_tree_idx
        FD_ISEDITABLE,    // FIELD_ring_end_server_idx
        FD_ISEDITABLE,    // FIELD_ring_end_gpu_idx
        FD_ISEDITABLE,    // FIELD_optireduce_in_reduction_phase
        FD_ISEDITABLE,    // FIELD_tree_search_type
        FD_ISEDITABLE,    // FIELD_tree_traversal_dir
        FD_ISEDITABLE,    // FIELD_collective_type
        FD_ISEDITABLE,    // FIELD_collective_alg_type
        FD_ISEDITABLE,    // FIELD_ina_leaf_aggregation_num
        FD_ISEDITABLE,    // FIELD_ina_spine_aggregation_num
        FD_ISEDITABLE,    // FIELD_ina_core_aggregation_num
        FD_ISEDITABLE,    // FIELD_seq_num
        FD_ISEDITABLE,    // FIELD_collective_scale
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_rsbf_seed_list
        FD_ISARRAY | FD_ISEDITABLE,    // FIELD_rsbf_bitstream_list
        FD_ISEDITABLE,    // FIELD_rsbf_removed_bytes
        FD_ISEDITABLE,    // FIELD_init_tree_pkt_idx
        FD_ISEDITABLE,    // FIELD_dst_tor_idx
        FD_ISEDITABLE,    // FIELD_partial_mcast_original_flow_size_bytes
        0,    // FIELD_controller_setup_finish_time
        FD_ISEDITABLE,    // FIELD_num_msgs_in_tor_group
        FD_ISEDITABLE,    // FIELD_tor_group_idx
        FD_ISEDITABLE,    // FIELD_agg_cidr_group_id
        FD_ISEDITABLE,    // FIELD_agg_cidr_group_member_count
        FD_ISEDITABLE,    // FIELD_core_cidr_group_id
        FD_ISEDITABLE,    // FIELD_core_cidr_group_member_count
        FD_ISEDITABLE,    // FIELD_in_src_sharding
        FD_ISEDITABLE,    // FIELD_using_orca
        FD_ISEDITABLE,    // FIELD_using_elmo
        FD_ISEDITABLE,    // FIELD_useful_data_bytes
        FD_ISEDITABLE,    // FIELD_elmo_overhead_pop_bytes
    };
    return (field >= 0 && field < 52) ? fieldTypeFlags[field] : 0;
}

const char *GenericAppMsgDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldName(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldNames[] = {
        "expectedReplyLength",
        "replyDelay",
        "serverClose",
        "requesterID",
        "is_micro_burst_flow",
        "query_id",
        "requested_time",
        "total_flow_size",
        "responsibility_list",
        "responsibility_gpu_list",
        "responsibility_flow_id_list",
        "ports_to_dest_idx",
        "jump_to_idx",
        "dst_idx_list",
        "flow_ids_list",
        "app_name",
        "app_full_path",
        "src_gpu_idx",
        "dst_gpu_idx",
        "all_ports_to_dst",
        "all_jump_to_idx",
        "current_tree_idx",
        "ring_end_server_idx",
        "ring_end_gpu_idx",
        "optireduce_in_reduction_phase",
        "tree_search_type",
        "tree_traversal_dir",
        "collective_type",
        "collective_alg_type",
        "ina_leaf_aggregation_num",
        "ina_spine_aggregation_num",
        "ina_core_aggregation_num",
        "seq_num",
        "collective_scale",
        "rsbf_seed_list",
        "rsbf_bitstream_list",
        "rsbf_removed_bytes",
        "init_tree_pkt_idx",
        "dst_tor_idx",
        "partial_mcast_original_flow_size_bytes",
        "controller_setup_finish_time",
        "num_msgs_in_tor_group",
        "tor_group_idx",
        "agg_cidr_group_id",
        "agg_cidr_group_member_count",
        "core_cidr_group_id",
        "core_cidr_group_member_count",
        "in_src_sharding",
        "using_orca",
        "using_elmo",
        "useful_data_bytes",
        "elmo_overhead_pop_bytes",
    };
    return (field >= 0 && field < 52) ? fieldNames[field] : nullptr;
}

int GenericAppMsgDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    int base = basedesc ? basedesc->getFieldCount() : 0;
    if (fieldName[0] == 'e' && strcmp(fieldName, "expectedReplyLength") == 0) return base+0;
    if (fieldName[0] == 'r' && strcmp(fieldName, "replyDelay") == 0) return base+1;
    if (fieldName[0] == 's' && strcmp(fieldName, "serverClose") == 0) return base+2;
    if (fieldName[0] == 'r' && strcmp(fieldName, "requesterID") == 0) return base+3;
    if (fieldName[0] == 'i' && strcmp(fieldName, "is_micro_burst_flow") == 0) return base+4;
    if (fieldName[0] == 'q' && strcmp(fieldName, "query_id") == 0) return base+5;
    if (fieldName[0] == 'r' && strcmp(fieldName, "requested_time") == 0) return base+6;
    if (fieldName[0] == 't' && strcmp(fieldName, "total_flow_size") == 0) return base+7;
    if (fieldName[0] == 'r' && strcmp(fieldName, "responsibility_list") == 0) return base+8;
    if (fieldName[0] == 'r' && strcmp(fieldName, "responsibility_gpu_list") == 0) return base+9;
    if (fieldName[0] == 'r' && strcmp(fieldName, "responsibility_flow_id_list") == 0) return base+10;
    if (fieldName[0] == 'p' && strcmp(fieldName, "ports_to_dest_idx") == 0) return base+11;
    if (fieldName[0] == 'j' && strcmp(fieldName, "jump_to_idx") == 0) return base+12;
    if (fieldName[0] == 'd' && strcmp(fieldName, "dst_idx_list") == 0) return base+13;
    if (fieldName[0] == 'f' && strcmp(fieldName, "flow_ids_list") == 0) return base+14;
    if (fieldName[0] == 'a' && strcmp(fieldName, "app_name") == 0) return base+15;
    if (fieldName[0] == 'a' && strcmp(fieldName, "app_full_path") == 0) return base+16;
    if (fieldName[0] == 's' && strcmp(fieldName, "src_gpu_idx") == 0) return base+17;
    if (fieldName[0] == 'd' && strcmp(fieldName, "dst_gpu_idx") == 0) return base+18;
    if (fieldName[0] == 'a' && strcmp(fieldName, "all_ports_to_dst") == 0) return base+19;
    if (fieldName[0] == 'a' && strcmp(fieldName, "all_jump_to_idx") == 0) return base+20;
    if (fieldName[0] == 'c' && strcmp(fieldName, "current_tree_idx") == 0) return base+21;
    if (fieldName[0] == 'r' && strcmp(fieldName, "ring_end_server_idx") == 0) return base+22;
    if (fieldName[0] == 'r' && strcmp(fieldName, "ring_end_gpu_idx") == 0) return base+23;
    if (fieldName[0] == 'o' && strcmp(fieldName, "optireduce_in_reduction_phase") == 0) return base+24;
    if (fieldName[0] == 't' && strcmp(fieldName, "tree_search_type") == 0) return base+25;
    if (fieldName[0] == 't' && strcmp(fieldName, "tree_traversal_dir") == 0) return base+26;
    if (fieldName[0] == 'c' && strcmp(fieldName, "collective_type") == 0) return base+27;
    if (fieldName[0] == 'c' && strcmp(fieldName, "collective_alg_type") == 0) return base+28;
    if (fieldName[0] == 'i' && strcmp(fieldName, "ina_leaf_aggregation_num") == 0) return base+29;
    if (fieldName[0] == 'i' && strcmp(fieldName, "ina_spine_aggregation_num") == 0) return base+30;
    if (fieldName[0] == 'i' && strcmp(fieldName, "ina_core_aggregation_num") == 0) return base+31;
    if (fieldName[0] == 's' && strcmp(fieldName, "seq_num") == 0) return base+32;
    if (fieldName[0] == 'c' && strcmp(fieldName, "collective_scale") == 0) return base+33;
    if (fieldName[0] == 'r' && strcmp(fieldName, "rsbf_seed_list") == 0) return base+34;
    if (fieldName[0] == 'r' && strcmp(fieldName, "rsbf_bitstream_list") == 0) return base+35;
    if (fieldName[0] == 'r' && strcmp(fieldName, "rsbf_removed_bytes") == 0) return base+36;
    if (fieldName[0] == 'i' && strcmp(fieldName, "init_tree_pkt_idx") == 0) return base+37;
    if (fieldName[0] == 'd' && strcmp(fieldName, "dst_tor_idx") == 0) return base+38;
    if (fieldName[0] == 'p' && strcmp(fieldName, "partial_mcast_original_flow_size_bytes") == 0) return base+39;
    if (fieldName[0] == 'c' && strcmp(fieldName, "controller_setup_finish_time") == 0) return base+40;
    if (fieldName[0] == 'n' && strcmp(fieldName, "num_msgs_in_tor_group") == 0) return base+41;
    if (fieldName[0] == 't' && strcmp(fieldName, "tor_group_idx") == 0) return base+42;
    if (fieldName[0] == 'a' && strcmp(fieldName, "agg_cidr_group_id") == 0) return base+43;
    if (fieldName[0] == 'a' && strcmp(fieldName, "agg_cidr_group_member_count") == 0) return base+44;
    if (fieldName[0] == 'c' && strcmp(fieldName, "core_cidr_group_id") == 0) return base+45;
    if (fieldName[0] == 'c' && strcmp(fieldName, "core_cidr_group_member_count") == 0) return base+46;
    if (fieldName[0] == 'i' && strcmp(fieldName, "in_src_sharding") == 0) return base+47;
    if (fieldName[0] == 'u' && strcmp(fieldName, "using_orca") == 0) return base+48;
    if (fieldName[0] == 'u' && strcmp(fieldName, "using_elmo") == 0) return base+49;
    if (fieldName[0] == 'u' && strcmp(fieldName, "useful_data_bytes") == 0) return base+50;
    if (fieldName[0] == 'e' && strcmp(fieldName, "elmo_overhead_pop_bytes") == 0) return base+51;
    return basedesc ? basedesc->findField(fieldName) : -1;
}

const char *GenericAppMsgDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldTypeString(field);
        field -= basedesc->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "inet::B",    // FIELD_expectedReplyLength
        "double",    // FIELD_replyDelay
        "bool",    // FIELD_serverClose
        "unsigned long",    // FIELD_requesterID
        "bool",    // FIELD_is_micro_burst_flow
        "unsigned long",    // FIELD_query_id
        "omnetpp::simtime_t",    // FIELD_requested_time
        "inet::b",    // FIELD_total_flow_size
        "int",    // FIELD_responsibility_list
        "int",    // FIELD_responsibility_gpu_list
        "unsigned long",    // FIELD_responsibility_flow_id_list
        "inet::InnerList",    // FIELD_ports_to_dest_idx
        "inet::InnerList",    // FIELD_jump_to_idx
        "int",    // FIELD_dst_idx_list
        "unsigned long",    // FIELD_flow_ids_list
        "string",    // FIELD_app_name
        "string",    // FIELD_app_full_path
        "int",    // FIELD_src_gpu_idx
        "int",    // FIELD_dst_gpu_idx
        "inet::TwoDInnerList",    // FIELD_all_ports_to_dst
        "inet::TwoDInnerList",    // FIELD_all_jump_to_idx
        "int",    // FIELD_current_tree_idx
        "int",    // FIELD_ring_end_server_idx
        "int",    // FIELD_ring_end_gpu_idx
        "bool",    // FIELD_optireduce_in_reduction_phase
        "int",    // FIELD_tree_search_type
        "int",    // FIELD_tree_traversal_dir
        "int",    // FIELD_collective_type
        "int",    // FIELD_collective_alg_type
        "int",    // FIELD_ina_leaf_aggregation_num
        "int",    // FIELD_ina_spine_aggregation_num
        "int",    // FIELD_ina_core_aggregation_num
        "unsigned long",    // FIELD_seq_num
        "unsigned int",    // FIELD_collective_scale
        "unsigned int",    // FIELD_rsbf_seed_list
        "string",    // FIELD_rsbf_bitstream_list
        "unsigned long",    // FIELD_rsbf_removed_bytes
        "int",    // FIELD_init_tree_pkt_idx
        "int",    // FIELD_dst_tor_idx
        "unsigned long",    // FIELD_partial_mcast_original_flow_size_bytes
        "omnetpp::simtime_t",    // FIELD_controller_setup_finish_time
        "int",    // FIELD_num_msgs_in_tor_group
        "int",    // FIELD_tor_group_idx
        "string",    // FIELD_agg_cidr_group_id
        "unsigned long",    // FIELD_agg_cidr_group_member_count
        "string",    // FIELD_core_cidr_group_id
        "unsigned long",    // FIELD_core_cidr_group_member_count
        "bool",    // FIELD_in_src_sharding
        "bool",    // FIELD_using_orca
        "bool",    // FIELD_using_elmo
        "int",    // FIELD_useful_data_bytes
        "int",    // FIELD_elmo_overhead_pop_bytes
    };
    return (field >= 0 && field < 52) ? fieldTypeStrings[field] : nullptr;
}

const char **GenericAppMsgDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldPropertyNames(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *GenericAppMsgDescriptor::getFieldProperty(int field, const char *propertyname) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldProperty(field, propertyname);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int GenericAppMsgDescriptor::getFieldArraySize(void *object, int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldArraySize(object, field);
        field -= basedesc->getFieldCount();
    }
    GenericAppMsg *pp = (GenericAppMsg *)object; (void)pp;
    switch (field) {
        case FIELD_responsibility_list: return pp->getResponsibility_listArraySize();
        case FIELD_responsibility_gpu_list: return pp->getResponsibility_gpu_listArraySize();
        case FIELD_responsibility_flow_id_list: return pp->getResponsibility_flow_id_listArraySize();
        case FIELD_ports_to_dest_idx: return pp->getPorts_to_dest_idxArraySize();
        case FIELD_jump_to_idx: return pp->getJump_to_idxArraySize();
        case FIELD_dst_idx_list: return pp->getDst_idx_listArraySize();
        case FIELD_flow_ids_list: return pp->getFlow_ids_listArraySize();
        case FIELD_all_ports_to_dst: return pp->getAll_ports_to_dstArraySize();
        case FIELD_all_jump_to_idx: return pp->getAll_jump_to_idxArraySize();
        case FIELD_rsbf_seed_list: return pp->getRsbf_seed_listArraySize();
        case FIELD_rsbf_bitstream_list: return pp->getRsbf_bitstream_listArraySize();
        default: return 0;
    }
}

const char *GenericAppMsgDescriptor::getFieldDynamicTypeString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldDynamicTypeString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    GenericAppMsg *pp = (GenericAppMsg *)object; (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string GenericAppMsgDescriptor::getFieldValueAsString(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldValueAsString(object,field,i);
        field -= basedesc->getFieldCount();
    }
    GenericAppMsg *pp = (GenericAppMsg *)object; (void)pp;
    switch (field) {
        case FIELD_expectedReplyLength: return unit2string(pp->getExpectedReplyLength());
        case FIELD_replyDelay: return double2string(pp->getReplyDelay());
        case FIELD_serverClose: return bool2string(pp->getServerClose());
        case FIELD_requesterID: return ulong2string(pp->getRequesterID());
        case FIELD_is_micro_burst_flow: return bool2string(pp->getIs_micro_burst_flow());
        case FIELD_query_id: return ulong2string(pp->getQuery_id());
        case FIELD_requested_time: return simtime2string(pp->getRequested_time());
        case FIELD_total_flow_size: return unit2string(pp->getTotal_flow_size());
        case FIELD_responsibility_list: return long2string(pp->getResponsibility_list(i));
        case FIELD_responsibility_gpu_list: return long2string(pp->getResponsibility_gpu_list(i));
        case FIELD_responsibility_flow_id_list: return ulong2string(pp->getResponsibility_flow_id_list(i));
        case FIELD_ports_to_dest_idx: {std::stringstream out; out << pp->getPorts_to_dest_idx(i); return out.str();}
        case FIELD_jump_to_idx: {std::stringstream out; out << pp->getJump_to_idx(i); return out.str();}
        case FIELD_dst_idx_list: return long2string(pp->getDst_idx_list(i));
        case FIELD_flow_ids_list: return ulong2string(pp->getFlow_ids_list(i));
        case FIELD_app_name: return oppstring2string(pp->getApp_name());
        case FIELD_app_full_path: return oppstring2string(pp->getApp_full_path());
        case FIELD_src_gpu_idx: return long2string(pp->getSrc_gpu_idx());
        case FIELD_dst_gpu_idx: return long2string(pp->getDst_gpu_idx());
        case FIELD_all_ports_to_dst: {std::stringstream out; out << pp->getAll_ports_to_dst(i); return out.str();}
        case FIELD_all_jump_to_idx: {std::stringstream out; out << pp->getAll_jump_to_idx(i); return out.str();}
        case FIELD_current_tree_idx: return long2string(pp->getCurrent_tree_idx());
        case FIELD_ring_end_server_idx: return long2string(pp->getRing_end_server_idx());
        case FIELD_ring_end_gpu_idx: return long2string(pp->getRing_end_gpu_idx());
        case FIELD_optireduce_in_reduction_phase: return bool2string(pp->getOptireduce_in_reduction_phase());
        case FIELD_tree_search_type: return long2string(pp->getTree_search_type());
        case FIELD_tree_traversal_dir: return long2string(pp->getTree_traversal_dir());
        case FIELD_collective_type: return long2string(pp->getCollective_type());
        case FIELD_collective_alg_type: return long2string(pp->getCollective_alg_type());
        case FIELD_ina_leaf_aggregation_num: return long2string(pp->getIna_leaf_aggregation_num());
        case FIELD_ina_spine_aggregation_num: return long2string(pp->getIna_spine_aggregation_num());
        case FIELD_ina_core_aggregation_num: return long2string(pp->getIna_core_aggregation_num());
        case FIELD_seq_num: return ulong2string(pp->getSeq_num());
        case FIELD_collective_scale: return ulong2string(pp->getCollective_scale());
        case FIELD_rsbf_seed_list: return ulong2string(pp->getRsbf_seed_list(i));
        case FIELD_rsbf_bitstream_list: return oppstring2string(pp->getRsbf_bitstream_list(i));
        case FIELD_rsbf_removed_bytes: return ulong2string(pp->getRsbf_removed_bytes());
        case FIELD_init_tree_pkt_idx: return long2string(pp->getInit_tree_pkt_idx());
        case FIELD_dst_tor_idx: return long2string(pp->getDst_tor_idx());
        case FIELD_partial_mcast_original_flow_size_bytes: return ulong2string(pp->getPartial_mcast_original_flow_size_bytes());
        case FIELD_controller_setup_finish_time: return simtime2string(pp->getController_setup_finish_time());
        case FIELD_num_msgs_in_tor_group: return long2string(pp->getNum_msgs_in_tor_group());
        case FIELD_tor_group_idx: return long2string(pp->getTor_group_idx());
        case FIELD_agg_cidr_group_id: return oppstring2string(pp->getAgg_cidr_group_id());
        case FIELD_agg_cidr_group_member_count: return ulong2string(pp->getAgg_cidr_group_member_count());
        case FIELD_core_cidr_group_id: return oppstring2string(pp->getCore_cidr_group_id());
        case FIELD_core_cidr_group_member_count: return ulong2string(pp->getCore_cidr_group_member_count());
        case FIELD_in_src_sharding: return bool2string(pp->getIn_src_sharding());
        case FIELD_using_orca: return bool2string(pp->getUsing_orca());
        case FIELD_using_elmo: return bool2string(pp->getUsing_elmo());
        case FIELD_useful_data_bytes: return long2string(pp->getUseful_data_bytes());
        case FIELD_elmo_overhead_pop_bytes: return long2string(pp->getElmo_overhead_pop_bytes());
        default: return "";
    }
}

bool GenericAppMsgDescriptor::setFieldValueAsString(void *object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->setFieldValueAsString(object,field,i,value);
        field -= basedesc->getFieldCount();
    }
    GenericAppMsg *pp = (GenericAppMsg *)object; (void)pp;
    switch (field) {
        case FIELD_expectedReplyLength: pp->setExpectedReplyLength(B(string2long(value))); return true;
        case FIELD_replyDelay: pp->setReplyDelay(string2double(value)); return true;
        case FIELD_serverClose: pp->setServerClose(string2bool(value)); return true;
        case FIELD_requesterID: pp->setRequesterID(string2ulong(value)); return true;
        case FIELD_is_micro_burst_flow: pp->setIs_micro_burst_flow(string2bool(value)); return true;
        case FIELD_query_id: pp->setQuery_id(string2ulong(value)); return true;
        case FIELD_total_flow_size: pp->setTotal_flow_size(b(string2long(value))); return true;
        case FIELD_responsibility_list: pp->setResponsibility_list(i,string2long(value)); return true;
        case FIELD_responsibility_gpu_list: pp->setResponsibility_gpu_list(i,string2long(value)); return true;
        case FIELD_responsibility_flow_id_list: pp->setResponsibility_flow_id_list(i,string2ulong(value)); return true;
        case FIELD_dst_idx_list: pp->setDst_idx_list(i,string2long(value)); return true;
        case FIELD_flow_ids_list: pp->setFlow_ids_list(i,string2ulong(value)); return true;
        case FIELD_app_name: pp->setApp_name((value)); return true;
        case FIELD_app_full_path: pp->setApp_full_path((value)); return true;
        case FIELD_src_gpu_idx: pp->setSrc_gpu_idx(string2long(value)); return true;
        case FIELD_dst_gpu_idx: pp->setDst_gpu_idx(string2long(value)); return true;
        case FIELD_current_tree_idx: pp->setCurrent_tree_idx(string2long(value)); return true;
        case FIELD_ring_end_server_idx: pp->setRing_end_server_idx(string2long(value)); return true;
        case FIELD_ring_end_gpu_idx: pp->setRing_end_gpu_idx(string2long(value)); return true;
        case FIELD_optireduce_in_reduction_phase: pp->setOptireduce_in_reduction_phase(string2bool(value)); return true;
        case FIELD_tree_search_type: pp->setTree_search_type(string2long(value)); return true;
        case FIELD_tree_traversal_dir: pp->setTree_traversal_dir(string2long(value)); return true;
        case FIELD_collective_type: pp->setCollective_type(string2long(value)); return true;
        case FIELD_collective_alg_type: pp->setCollective_alg_type(string2long(value)); return true;
        case FIELD_ina_leaf_aggregation_num: pp->setIna_leaf_aggregation_num(string2long(value)); return true;
        case FIELD_ina_spine_aggregation_num: pp->setIna_spine_aggregation_num(string2long(value)); return true;
        case FIELD_ina_core_aggregation_num: pp->setIna_core_aggregation_num(string2long(value)); return true;
        case FIELD_seq_num: pp->setSeq_num(string2ulong(value)); return true;
        case FIELD_collective_scale: pp->setCollective_scale(string2ulong(value)); return true;
        case FIELD_rsbf_seed_list: pp->setRsbf_seed_list(i,string2ulong(value)); return true;
        case FIELD_rsbf_bitstream_list: pp->setRsbf_bitstream_list(i,(value)); return true;
        case FIELD_rsbf_removed_bytes: pp->setRsbf_removed_bytes(string2ulong(value)); return true;
        case FIELD_init_tree_pkt_idx: pp->setInit_tree_pkt_idx(string2long(value)); return true;
        case FIELD_dst_tor_idx: pp->setDst_tor_idx(string2long(value)); return true;
        case FIELD_partial_mcast_original_flow_size_bytes: pp->setPartial_mcast_original_flow_size_bytes(string2ulong(value)); return true;
        case FIELD_num_msgs_in_tor_group: pp->setNum_msgs_in_tor_group(string2long(value)); return true;
        case FIELD_tor_group_idx: pp->setTor_group_idx(string2long(value)); return true;
        case FIELD_agg_cidr_group_id: pp->setAgg_cidr_group_id((value)); return true;
        case FIELD_agg_cidr_group_member_count: pp->setAgg_cidr_group_member_count(string2ulong(value)); return true;
        case FIELD_core_cidr_group_id: pp->setCore_cidr_group_id((value)); return true;
        case FIELD_core_cidr_group_member_count: pp->setCore_cidr_group_member_count(string2ulong(value)); return true;
        case FIELD_in_src_sharding: pp->setIn_src_sharding(string2bool(value)); return true;
        case FIELD_using_orca: pp->setUsing_orca(string2bool(value)); return true;
        case FIELD_using_elmo: pp->setUsing_elmo(string2bool(value)); return true;
        case FIELD_useful_data_bytes: pp->setUseful_data_bytes(string2long(value)); return true;
        case FIELD_elmo_overhead_pop_bytes: pp->setElmo_overhead_pop_bytes(string2long(value)); return true;
        default: return false;
    }
}

const char *GenericAppMsgDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructName(field);
        field -= basedesc->getFieldCount();
    }
    switch (field) {
        case FIELD_ports_to_dest_idx: return omnetpp::opp_typename(typeid(InnerList));
        case FIELD_jump_to_idx: return omnetpp::opp_typename(typeid(InnerList));
        case FIELD_all_ports_to_dst: return omnetpp::opp_typename(typeid(TwoDInnerList));
        case FIELD_all_jump_to_idx: return omnetpp::opp_typename(typeid(TwoDInnerList));
        default: return nullptr;
    };
}

void *GenericAppMsgDescriptor::getFieldStructValuePointer(void *object, int field, int i) const
{
    omnetpp::cClassDescriptor *basedesc = getBaseClassDescriptor();
    if (basedesc) {
        if (field < basedesc->getFieldCount())
            return basedesc->getFieldStructValuePointer(object, field, i);
        field -= basedesc->getFieldCount();
    }
    GenericAppMsg *pp = (GenericAppMsg *)object; (void)pp;
    switch (field) {
        case FIELD_ports_to_dest_idx: return toVoidPtr(&pp->getPorts_to_dest_idx(i)); break;
        case FIELD_jump_to_idx: return toVoidPtr(&pp->getJump_to_idx(i)); break;
        case FIELD_all_ports_to_dst: return toVoidPtr(&pp->getAll_ports_to_dst(i)); break;
        case FIELD_all_jump_to_idx: return toVoidPtr(&pp->getAll_jump_to_idx(i)); break;
        default: return nullptr;
    }
}

} // namespace inet

