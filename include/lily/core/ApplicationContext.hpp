#ifndef LILY_CORE_APPLICATION_CONTEXT_HPP
#define LILY_CORE_APPLICATION_CONTEXT_HPP

#include <memory>
#include <map>
#include <functional>
#include <stdexcept>
#include <mutex>

namespace lily {
namespace core {

/**
 * @brief Dependency Injection Container
 * 
 * Similar to Spring's ApplicationContext, this class
 * manages dependency injection and bean lifecycle.
 */
class ApplicationContext {
public:
    using CreateFunction = std::function<std::shared_ptr<void>()>;
    
    ApplicationContext() = default;
    
    /**
     * @brief Register a bean (similar to @Bean annotation)
     */
    template<typename T>
    void registerBean(const std::string& name, std::shared_ptr<T> instance) {
        std::lock_guard<std::mutex> lock(mutex_);
        beans_[name] = instance;
    }
    
    /**
     * @brief Register a bean factory (similar to @Bean with factory method)
     */
    template<typename T>
    void registerBeanFactory(const std::string& name, CreateFunction factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        factories_[name] = factory;
    }
    
    /**
     * @brief Get bean by type (similar to Autowiring by type)
     */
    template<typename T>
    std::shared_ptr<T> getBean() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // First try exact match
        for (const auto& pair : beans_) {
            if (auto casted = std::dynamic_pointer_cast<T>(pair.second)) {
                return casted;
            }
        }
        
        // Then try factory
        for (const auto& pair : factories_) {
            if (auto factory = std::any_cast<CreateFunction>(&pair.second)) {
                // This won't work directly, simplified version
            }
        }
        
        return nullptr;
    }
    
    /**
     * @brief Get bean by name (similar to getBean(String name))
     */
    template<typename T>
    std::shared_ptr<T> getBeanByName(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = beans_.find(name);
        if (it != beans_.end()) {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        
        return nullptr;
    }
    
    /**
     * @brief Check if bean exists (similar to containsBean)
     */
    bool containsBean(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return beans_.find(name) != beans_.end() || factories_.find(name) != factories_.end();
    }
    
    /**
     * @brief Get all bean names (similar to getBeanDefinitionNames)
     */
    std::vector<std::string> getBeanNames() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& pair : beans_) {
            names.push_back(pair.first);
        }
        for (const auto& pair : factories_) {
            names.push_back(pair.first);
        }
        return names;
    }

private:
    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<void>> beans_;
    std::map<std::string, CreateFunction> factories_;
};

/**
 * @brief Bean reference holder
 * 
 * Similar to @Autowired annotation, this holds a reference
 * that will be resolved later.
 */
template<typename T>
class Autowire {
public:
    Autowire() : bean_(nullptr) {}
    
    Autowire(std::shared_ptr<T> bean) : bean_(bean) {}
    
    T* operator->() { return bean_.get(); }
    T& operator*() { return *bean_; }
    
    std::shared_ptr<T> get() { return bean_; }
    
    void set(std::shared_ptr<T> bean) { bean_ = bean; }
    
    bool empty() const { return bean_ == nullptr; }
    
private:
    std::shared_ptr<T> bean_;
};

/**
 * @brief Application context holder
 * 
 * Similar to Spring's ApplicationContextAware.
 */
class ApplicationContextHolder {
public:
    static void setContext(std::shared_ptr<ApplicationContext> context) {
        context_ = context;
    }
    
    static std::shared_ptr<ApplicationContext> getContext() {
        return context_;
    }
    
    template<typename T>
    static std::shared_ptr<T> getBean() {
        if (context_) {
            return context_->getBean<T>();
        }
        return nullptr;
    }

private:
    static std::shared_ptr<ApplicationContext> context_;
};

} // namespace core
} // namespace lily

#endif // LILY_CORE_APPLICATION_CONTEXT_HPP
