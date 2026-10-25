-- Executable statistics from MetaNum/numeric.hpp; catalog() returns metadata only.
local native=require('metanum_native')
local M={catalog=native.catalog,max_samples=native.max_samples}
for _,name in ipairs({'mean','median','min','max'}) do
  M[name]=function(x) return native.compute(name,x,nil,nil) end
end
for _,name in ipairs({'variance','standard_deviation'}) do
  M[name]=function(x,sample)
    if sample==nil then sample=true end
    return native.compute(name,x,nil,sample)
  end
end
function M.covariance(x,y,sample)
  if sample==nil then sample=true end
  return native.compute('covariance',x,y,sample)
end
function M.correlation(x,y) return native.compute('correlation',x,y,nil) end
function M.quantile(x,p) return native.compute('quantile',x,nil,p) end
return M
