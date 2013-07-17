//$Id: Record.h 3513 2013-06-29 00:27:49Z jamshid.esmaelnezhad $

/*
 * The Software is made available solely for use according to the License Agreement. Any reproduction
 * or redistribution of the Software not in accordance with the License Agreement is expressly prohibited
 * by law, and may result in severe civil and criminal penalties. Violators will be prosecuted to the
 * maximum extent possible.
 *
 * THE SOFTWARE IS WARRANTED, IF AT ALL, ONLY ACCORDING TO THE TERMS OF THE LICENSE AGREEMENT. EXCEPT
 * AS WARRANTED IN THE LICENSE AGREEMENT, SRCH2 INC. HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS WITH
 * REGARD TO THE SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES AND CONDITIONS OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT.  IN NO EVENT SHALL SRCH2 INC. BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF SOFTWARE.

 * Copyright © 2010 SRCH2 Inc. All rights reserved
 */

#ifndef __RECORD_H__
#define __RECORD_H__

#include <instantsearch/platform.h>

#include <string>

namespace srch2
{
namespace instantsearch
{
class Schema;

typedef enum {
    LU_PRESENT_IN_READVIEW_AND_WRITEVIEW,
    LU_TO_BE_INSERTED,
    LU_ABSENT_OR_TO_BE_DELETED
} INDEXLOOKUP_RETVAL;

/**
 * This class defines a data record passed to the Indexer. The Schema
 * class defines the structure of the record.
 */
class MYLIB_EXPORT Record
{
public:

    /**
     * @param schema A const reference to the Schema of the record.
     */
    Record(const Schema *schema);

    /** 
     * Sets the primary key value.
     *  @param primaryKey Value of the primary key
     */
    void setPrimaryKey(const std::string &primaryKey);
    void setPrimaryKey(const unsigned &primaryKey);

    /** 
     * Gets the primary key value.
     *  \returns Value of the primary key.
     */
    const std::string& getPrimaryKey() const;

    /**
     * \returns the const reference to the Schema associated with the record.
     */
    const Schema* getSchema() const;

    /**
     * Set the value of an attribute.
     *
     * These attributes are searched in query time.
     *
     * @param[in] attributeName name of the Attribute;
     * @param[in] attributeValue  text string for the Attribute.
     */
    bool setSearchableAttributeValue(const std::string &attributeName,
            const std::string &attributeValue);

    /**
     * Sets the value of the attribute given its index in the list
     * of attributes in the schema.
     *
     * @param attributeId The index of an attribute in the schema.
     * @param attributeValue The text string of this attribute.
     */
    bool setSearchableAttributeValue(const unsigned attributeId,
            const std::string &attributeValue);

    /**
     * Set the value of an attribute.
     *
     * These attributes are used to sort the results
     * and must have numerical values.
     *
     * @param[in] attributeName name of the Attribute;
     * @param[in] attributeValue  text string for the Attribute.
     */
    bool setSortableAttributeValue(const std::string &attributeName,
                const std::string &attributeValue);


    /**
     * Set the value of an attribute.
     *
     * These attributes are used to sort the results
     * and must have numerical values.
     *
     * @param[in] attributeName name of the Attribute;
     * @param[in] attributeValue  text string for the Attribute.
     */
    bool setNonSearchableAttributeValue(const std::string &attributeName,
                const std::string &attributeValue);

    /**
     * Sets the value of the attribute given its index in the list
     * of attributes in the schema.
     *
     * @param attributeId The index of an attribute in the schema.
     * @param attributeValue The text string of this attribute.
     */
    bool setNonSearchableAttributeValue(const unsigned attributeId,
                    const std::string &attributeValue);

    /**
     * Returns the value of the attribute corresponding to the attributeId.
     */
    std::string *getSearchableAttributeValue(const unsigned attributeId) const;


    std::string *getNonSearchableAttributeValue(const unsigned attributeId) const;

    /**
     *
     * Sets the boost value of this record in the range [0 - 100].
     *
     * @param record boost value
     */
    void setRecordBoost(const float recordBoost);

    /*
     * It is a compressed copy of data which is kept in memory.
     *
     * @param the compressed copy of data
     */
    void setInMemoryData(const std::string &inMemoryRecordString);
    std::string getInMemoryData() const;

    /**
     * Gets the boost value of this record in the range [0 - 100].
     */
    float getRecordBoost() const;

    /**
     * Sets the location attribute value of the record.
     */
    void setLocationAttributeValue(const double &latitude, const double &longitude);

    /**
     * Gets the location attribute value of the record.
     */
    std::pair<double,double> getLocationAttributeValue() const;

    /**
     *  Clears the content of the record except the schema
     *  pointer.
     */
    void clear();

    /**
     * Destructor to free persistent resources used by the Record.
     */
    virtual ~Record();

private:
    struct Impl;
    Impl *impl;
};
}}
#endif //__RECORD_H__