
namespace securitylib {
    
    void setApacheAcl(std::string const& name, bool isNameDirectory)
    {
        throw ERROR_EXCEPTION << "setApacheAcl function is not supported on Aix." << '\n';
    }

} // namespace securitylib
