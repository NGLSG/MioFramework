device = nil
img1 = nil
img2 = nil
img3 = nil
function Awake()

end

function Start(client)
    if client == nil then
        print("client is nil")
        return ;
    else
        img1 = ImageUtils.Image("Resources/assets/img/1999/1.jpg")
        img2 = ImageUtils.Image("Resources/assets/img/1999/2.jpg")
        img3 = ImageUtils.Image("Resources/assets/img/1999/3.jpg")
        device = client
        point = Point:new(-1, -1)
        while point.x == -1 and point.y == -1 do
            screen = ImageUtils.PrintScreen(device)
            point = ImageUtils.FindFromMat(screen, img1)
            Sleep(100)
        end
        device:tap(point)

        point = Point:new(-1, -1)
        while point.x == -1 and point.y == -1 do
            screen = ImageUtils.PrintScreen(device)
            point = ImageUtils.FindFromMat(screen, img2)
            Sleep(100)
        end
        device:tap(point)

        point = Point:new(-1, -1)
        while point.x == -1 and point.y == -1 do
            screen = ImageUtils.PrintScreen(device)
            point = ImageUtils.FindFromMat(screen, img3)
            Sleep(100)
        end
        device:tap(point)
    end
end

function Update()

end

function OnDestroy()

end