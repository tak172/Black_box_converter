namespace boost {
    template scoped_ptr<class T> 
    class scoped_ptr : public T*
    {
    public:
        ~scoped_ptr() // never throws
        {
            delete ((T*)(this));
        }
        void reset(T * p = 0)
        {
            *this= p;
        }
        T * get() const
        {
            return this;
        }
    };

    #define shared_ptr  scoped_ptr
};
