class SourceIdAttributeGroupObject
{
	const char *m_pName;
	const char *m_pHostId1AttrName;
	const char *m_pHostId2AttrName;
public:
	SourceIdAttributeGroupObject();
	const char * GetName(void);
	const char * GetHostId1AttrName(void);
	const char * GetHostId2AttrName(void);
};

inline const char * SourceIdAttributeGroupObject::GetName(void)
{
	return m_pName;
}
inline const char * SourceIdAttributeGroupObject::GetHostId1AttrName(void)
{
	return m_pHostId1AttrName;
}
inline const char * SourceIdAttributeGroupObject::GetHostId2AttrName(void)
{
	return m_pHostId2AttrName;
}