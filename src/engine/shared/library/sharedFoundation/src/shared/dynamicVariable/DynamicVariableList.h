//========================================================================
//
// DynamicVariableList.h
//
// copyright 2001 Sony Online Entertainment
//
//========================================================================

#ifndef _INCLUDED_DynamicVariableList_H
#define _INCLUDED_DynamicVariableList_H

//========================================================================
// class DynamicVariableList - a list of DynamicVariables

#include "Archive/AutoDeltaMap.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/DynamicVariable.h"

//@todo: this should be removed
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/DynamicVariableListNestedList.h"

const char LIST_SEPARATOR = '.';
class DynamicVariableListNestedList;
class NetworkId;
class StringId;
class Transform;
class Vector;

// ======================================================================

/**
 * A DynamicVariableList is a map of name, value pairs.  The values can have many different types.
 */
class DynamicVariableList
{
public:
	typedef Archive::AutoDeltaMap<std::string, DynamicVariable> MapType;
	typedef DynamicVariableListNestedList NestedList;

public:
	DynamicVariableList();
	~DynamicVariableList();

	MapType::const_iterator                begin            () const;
	MapType::const_iterator                end              () const;
	MapType::const_iterator                lower_bound      (const std::string &name) const;
	MapType::const_iterator                find             (const std::string &name) const;
	void                                   copy             (const std::string &destObjvarName, const DynamicVariableList &sourceList, const std::string &sourceObjvarName);
	bool                                   removeItemByName (const std::string &name);
	bool                                   hasItem          (const std::string &name) const;
	DynamicVariable:: DynamicVariableType  getType          (const std::string &name) const; // for compatibility, should return LIST if the item is a sublist
	void                                   addToPackage     (Archive::AutoDeltaByteStream & bs, Archive::AutoDeltaByteStream & bs_np);
	size_t size() const;

	bool getItem(const std::string &name,int & value) const;
	bool getItem(const std::string &name,stdvector<int>::fwd & value) const;
	bool getItem(const std::string &name,float & value) const;
	bool getItem(const std::string &name,stdvector<float>::fwd & value) const;
	bool getItem(const std::string &name,Unicode::String & value) const;
	bool getItem(const std::string &name,std::string & value) const;
	bool getItem(const std::string &name,stdvector<Unicode::String>::fwd & value) const;
	bool getItem(const std::string &name,NetworkId & value) const;
	bool getItem(const std::string &name,stdvector<NetworkId>::fwd & value) const;
	bool getItem(const std::string &name,DynamicVariableLocationData & value) const;
	bool getItem(const std::string &name,stdvector<DynamicVariableLocationData>::fwd & value) const;
	bool getItem(const std::string &name,StringId &value) const;
	bool getItem(const std::string &name,stdvector<StringId>::fwd &value) const;
	bool getItem(const std::string &name,Transform &value) const;
	bool getItem(const std::string &name,stdvector<Transform>::fwd &value) const;
	bool getItem(const std::string &name,Vector &value) const;
	bool getItem(const std::string &name,stdvector<Vector>::fwd &value) const;

	bool setItem(const std::string &name, const int & value);
	bool setItem(const std::string &name, const stdvector<int>::fwd & value);
	bool setItem(const std::string &name, const float & value);
	bool setItem(const std::string &name, const stdvector<float>::fwd & value);
	bool setItem(const std::string &name, const Unicode::String & value);
	bool setItem(const std::string &name, const std::string & value);
	bool setItem(const std::string &name, const stdvector<Unicode::String>::fwd & value);
	bool setItem(const std::string &name, const NetworkId & value);
	bool setItem(const std::string &name, const stdvector<NetworkId>::fwd & value);
	bool setItem(const std::string &name, const DynamicVariableLocationData & value);
	bool setItem(const std::string &name, const stdvector<DynamicVariableLocationData>::fwd & value);
	bool setItem(const std::string &name, const StringId &value);
	bool setItem(const std::string &name, const stdvector<StringId>::fwd &value);
	bool setItem(const std::string &name, const Transform &value);
	bool setItem(const std::string &name, const stdvector<Transform>::fwd &value);
	bool setItem(const std::string &name, const Vector &value);
	bool setItem(const std::string &name, const stdvector<Vector>::fwd &value);
	bool setItem(const std::string &name, DynamicVariable value);

	int getRevision() const;
	void updateRevision();
		
private:
	MapType m_map;
	Archive::AutoDeltaVariable<int> m_freePositions;
	Archive::AutoDeltaVariable<int> m_revision;

	int getFreePosition();
	void releasePosition(int positionId);
	
private:
	DynamicVariableList &operator=(const DynamicVariableList &); // disable

	friend class DynamicVariableListNestedList;
};

// ======================================================================

#endif
