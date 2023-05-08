class ExcludedVolumesAttributeGroupObject
{
	const char *m_pName;
	const char *m_pVolume1AttrName;
	const char *m_pVolume2AttrName;
public:
	ExcludedVolumesAttributeGroupObject();	
	const char * GetName(void);
	const char * GetVolume1AttrName(void);
	const char * GetVolume2AttrName(void);
};

inline const char * ExcludedVolumesAttributeGroupObject::GetName(void)
{
	return m_pName;
}

inline const char * ExcludedVolumesAttributeGroupObject::GetVolume1AttrName(void)
{
	return m_pVolume1AttrName;
}
inline const char * ExcludedVolumesAttributeGroupObject::GetVolume2AttrName(void)
{
	return m_pVolume2AttrName;
}
