#ifndef __MONITOR_HPP__
#define __MONITOR_HPP__



template<typename T, typename Mutex = std::mutex>
class Monitor
{
    private:
        mutable T t_;
        mutable Mutex m_;
    public:
        Monitor(T t = T{}): t_(t) {};

        template<typename F>
        auto operator()(F func) const
        {
            std::lock_guard<Mutex> lck(m_);
            return func(t_);
        }
};


#endif // __MONITOR_HPP__
