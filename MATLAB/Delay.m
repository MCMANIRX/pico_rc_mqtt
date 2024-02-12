classdef Delay < handle
    properties (Access = private)
        timestamp
        delay
    end
    
    methods
        function obj = Delay()
            obj.timestamp = 0;
            obj.delay = 0;
        end
        
        function result = timeout(obj)
            result = ((millis() - obj.timestamp) > obj.delay);
         %   disp()
        end
        
        function delay_ms(obj, delay)
            obj.timestamp = millis();
            obj.delay = delay;
        end
    end
end

function time = millis()
    time = int32( round(toc * 1000));
end

function delay_ms(delay)
    t0 = millis();
    while (millis() - t0) < delay
        % Do nothing, just wait
    end
end
