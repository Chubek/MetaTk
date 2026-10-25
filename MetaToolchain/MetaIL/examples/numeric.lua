local numeric=require('metanum')
local data={1,2,3,4}
print('mean',numeric.mean(data))
print('population variance',numeric.variance(data,false))
print('median',numeric.median(data))
print('25% quantile',numeric.quantile(data,0.25))
print('correlation',numeric.correlation(data,{8,6,4,2}))
print('metadata categories',#numeric.catalog())
