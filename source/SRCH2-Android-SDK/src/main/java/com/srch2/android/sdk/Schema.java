package com.srch2.android.sdk;

import java.util.HashSet;

/**
 * Defines an index's structure such as its data fields. If an index is like an SQLite database table, the schema
 * is the table structure and its fields are the columns of that table. The fields of a schema must be named and
 * typed; they can also have additional properties such as highlighting and facet. See {@link Field}
 * for more information.
 * <br><br>
 * A schema <b>should only</b> be constructed when returning from the <code>Indexable</code> implementation of {@link Indexable#getSchema()}.
 * A schema <b>should never</b> be initialized with null field values or duplicated fields.
 */
public final class Schema {

    final String uniqueKey;
    String recordBoostKey;
    HashSet<Field> fields;
    boolean facetEnabled = false;

    Schema(PrimaryKeyField primaryKeyField, Field... remainingField) {
        if (primaryKeyField == null) {
            throw new IllegalArgumentException(
                    "Value of the primary key field cannot be null");
        }

        uniqueKey = primaryKeyField.primaryKey.name;
        fields = new HashSet<Field>();
        fields.add(primaryKeyField.primaryKey);

        boolean searchableFieldExist = primaryKeyField.primaryKey.searchable;

        for (Field f : remainingField) {
            if (f == null) {
                throw new IllegalArgumentException(
                        "Value of the field cannot be null");
            }
            addToFields(f);
            if (!searchableFieldExist && f.searchable) {
                searchableFieldExist = true;
            }
        }
        if (!searchableFieldExist) {
            throw new IllegalArgumentException("No searchable field provided. The Engine need at least one searchable field to index on");
        }
    }

    Schema(PrimaryKeyField primaryKeyField, RecordBoostField recordBoostField, Field... remainingField) {
        if (recordBoostField == null) {
            throw new IllegalArgumentException(
                    "Value of the record boost field cannot be null");
        }

        uniqueKey = primaryKeyField.primaryKey.name;
        fields = new HashSet<Field>();
        fields.add(primaryKeyField.primaryKey);
        boolean searchableFieldExist = primaryKeyField.primaryKey.searchable;

        recordBoostKey = recordBoostField.recordBoost.name;
        fields.add(recordBoostField.recordBoost);

        for (Field f : remainingField) {
            if (f == null) {
                throw new IllegalArgumentException(
                        "Value of the field cannot be null");
            }
            addToFields(f);
            if (!searchableFieldExist && f.searchable) {
                searchableFieldExist = true;
            }
        }
        if (!searchableFieldExist) {
            throw new IllegalArgumentException("No searchable field provided. The Engine need at least one searchable field to index on");
        }
    }

     /**
     * Defines the structure of an index's data fields. If an index is like a table in the SQLite database,
     * the schema is the table structure and its fields are the columns of that table. This method should
     * only be called when
     * returning the from the <code>Indexable</code> implementation of the method
     * {@link Indexable#getSchema()}.
     * <br><br>
     * The first argument <b>should always</b> be the primary key field, which can be used to
     * retrieve a specific record or as the handle to delete the record from the index. Each
     * record <b>should have a unique value</b> for its primary key.
     * <br><br>
     * The remaining set of arguments are the rest of the schema's fields as they are defined
     * for the index. They can be passed in any order.
     * <br><br>
     * A schema initialized with null field values will cause an exception to be thrown when
     * {@link SRCH2Engine#initialize(Indexable, Indexable...)} is called.
     * @param primaryKeyField the field which will be the primary key of the index's schema
     * @param remainingField  the set of any other fields needed to define the schema
     */
    static public Schema createSchema(PrimaryKeyField primaryKeyField, Field... remainingField) {
        return new Schema(primaryKeyField, remainingField);
    }

    /**
     * Defines the structure of an index's data fields with one field specified as the record
     * boost field. If an index is like a table in the SQLite database,
     * the schema is the table structure and its fields are the columns of that table. This
     * method should
     * only be called when
     * returning the from the <code>Indexable</code> implementation of the method
     * {@link Indexable#getSchema()}.
     * <br><br>
     * The first argument <b>should always</b> be the primary key field, which can be used to
     * retrieve a specific record or as the handle to delete the record from the index. Each
     * record <b>should have a unique value</b> for its primary key.
     * <br><br>
     * The second argument <b>should always</b> be the record boost field, which can be used
     * to assign a float value as a score for each record. The default value is one for each
     * record if this is not set.
     * <br><br>
     * The remaining set of arguments are the rest of the schema's fields as they are defined
     * for the index. They can be passed in any order.
     * <br><br>
     * A schema initialized with null field values will cause an exception to be thrown when
     * {@link SRCH2Engine#initialize(Indexable, Indexable...)} is called.
     * @param primaryKeyField the field which will be the primary key of the index's schema
     * @param recordBoostField the field which will define the score or boost value to assign to the record
     * @param remainingField  the set of any other fields needed to define the schema
     */
    static public Schema createSchema(PrimaryKeyField primaryKeyField, RecordBoostField recordBoostField, Field... remainingField) {
        return new Schema(primaryKeyField, recordBoostField, remainingField);
    }

    Schema(PrimaryKeyField primaryKeyField, String latitudeFieldName,
           String longitudeFieldName, Field... remainingField) {
        this(primaryKeyField, remainingField);
        addToFields(new Field(latitudeFieldName, Field.InternalType.LOCATION_LATITUDE,
                false, false, Field.DEFAULT_BOOST_VALUE));
        addToFields(new Field(longitudeFieldName,
                Field.InternalType.LOCATION_LONGITUDE, false, false,
                Field.DEFAULT_BOOST_VALUE));
    }

    Schema(PrimaryKeyField primaryKeyField, RecordBoostField recordBoostField, String latitudeFieldName,
           String longitudeFieldName, Field... remainingField) {
        this(primaryKeyField, recordBoostField, remainingField);
        addToFields(new Field(latitudeFieldName, Field.InternalType.LOCATION_LATITUDE,
                false, false, Field.DEFAULT_BOOST_VALUE));
        addToFields(new Field(longitudeFieldName,
                Field.InternalType.LOCATION_LONGITUDE, false, false,
                Field.DEFAULT_BOOST_VALUE));
    }

    /**
     * Defines the structure of an index's data fields that is to include geosearch capability. If an index
     * is like a table in the SQLite database,
     * the schema is the table structure and its fields are the columns of that table. This method should
     * only be called when
     * returning the from the <code>Indexable</code> implementation of the method
     * {@link Indexable#getSchema()}.
     * <br><br>
     * The first argument <b>should always</b> be the primary key field, which can be used to
     * retrieve a specific record or as the handle to delete the record from the index. Each
     * record <b>should have a unique value</b> for its primary key.
     * <br><br>
     * The second and third arguments <b>should always</b> be the latitude and longitude
     * fields, in that order, that are defined for the index's schema.
     * <br><br>
     * The remaining set of arguments are the rest of the schema's fields as they are defined
     * for the index. They can be passed in any order.
     * <br><br>
     * A schema initialized with null field values will cause an exception to be thrown when
     * {@link SRCH2Engine#initialize(Indexable, Indexable...)} is called.
     * @param primaryKeyField the field which will be the primary key of the index's schema
     * @param latitudeFieldName the field which will be the latitude field of the index's schema
     * @param longitudeFieldName the field which will be the longitude field of the index's schema
     * @param remainingField  the set of any other fields needed to define the schema
     */
    static public Schema createGeoSchema(PrimaryKeyField primaryKeyField, String latitudeFieldName,
                                  String longitudeFieldName, Field... remainingField) {
        return new Schema(primaryKeyField, latitudeFieldName,
                longitudeFieldName, remainingField);
    }


    /**
     * Defines the structure of an index's data fields that is to include geosearch capability  with one field
     * specified as the record
     * boost field. If an index
     * is like a table in the SQLite database,
     * the schema is the table structure and its fields are the columns of that table. This method should
     * only be called when
     * returning the from the <code>Indexable</code> implementation of the method
     * {@link Indexable#getSchema()}.
     * <br><br>
     * The first argument <b>should always</b> be the primary key field, which can be used to
     * retrieve a specific record or as the handle to delete the record from the index. Each
     * record <b>should have a unique value</b> for its primary key.
     * <br><br>
     * The second argument <b>should always</b> be the record boost field, which can be used
     * to assign a float value as a score for each record. The default value is one for each
     * record if this is not set.
     * <br><br>
     * The third and fourth arguments <b>should always</b> be the latitude and longitude
     * fields, in that order, that are defined for the index's schema.
     * <br><br>
     * The remaining set of arguments are the rest of the schema's fields as they are defined
     * for the index. They can be passed in any order.
     * <br><br>
     * A schema initialized with null field values will cause an exception to be thrown when
     * {@link SRCH2Engine#initialize(Indexable, Indexable...)} is called.
     * @param primaryKeyField the field which will be the primary key of the index's schema
     * @param recordBoostField the field which will define the score or boost value to assign to the record
     * @param latitudeFieldName the field which will be the latitude field of the index's schema
     * @param longitudeFieldName the field which will be the longitude field of the index's schema
     * @param remainingField  the set of any other fields needed to define the schema
     */
    static public Schema createGeoSchema(PrimaryKeyField primaryKeyField, RecordBoostField recordBoostField,
                                            String latitudeFieldName, String longitudeFieldName, Field... remainingField) {
        return new Schema(primaryKeyField, recordBoostField, latitudeFieldName,
                longitudeFieldName, remainingField);
    }



    private void addToFields(Field f) {
        if (fields.contains(f)) {
            throw new IllegalArgumentException("Duplicated field with name: " + f.name);
        }
        fields.add(f);
    }



}
