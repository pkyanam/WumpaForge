/* The process owns one desktop GL context; capabilities are immutable across
 * its serialized callers. Query once without touching unsupported state enums. */
static int s_indexed_fixed_restart=-1;
static int indexed_fixed_restart_supported(void)
{
    if(s_indexed_fixed_restart>=0)return s_indexed_fixed_restart;
    GLint major=0,minor=0,count=0;
    glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
    s_indexed_fixed_restart=major>4||(major==4&&minor>=3);
    if(!s_indexed_fixed_restart) {
        glGetIntegerv(GL_NUM_EXTENSIONS,&count);
        for(GLint i=0;i<count;++i) {
            const char *extension=(const char *)glGetStringi(GL_EXTENSIONS,(GLuint)i);
            if(extension&&!strcmp(extension,"GL_ARB_ES3_compatibility")) {
                s_indexed_fixed_restart=1;break;
            }
        }
    }
    return s_indexed_fixed_restart;
}
