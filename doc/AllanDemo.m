function AllanDemo()

% ���������,RMS = 2.0,����BW=200Hz,��ô��Ӧ��sqrt(PSD)=RMS/sqrt(BW)
bw = 200;              % �����ʣ�����
time = 1:1/bw:10000;   % ��ʱ�� 10000 s
rms = 2.0;             % ������
white = rms * randn(length(time),1);    % ����������


% ����������ߣ�����ϵ����ǰ�������q��PSD��һ��
rw = cumsum(white);

% �������Ҳ�
omega = 0.01;
s = sin(time * 2*pi*omega);

close all; 
[av, tau] = Allan(white, 1/bw, 1.2);
AllanPlot(tau, av);

[av1, tau1] = Allan(rw, 1/bw, 1.2);
AllanPlot(tau1, av1);

[av2, tau2] = Allan(s, 1/bw, 1.02);
AllanPlot(tau2, av2);

function AllanPlot(tau, data)
figure,
loglog(tau,data,'-r','LineWidth',2,...
    'MarkerFaceColor','k','MarkerSize',8);  %�������ͣ���ɫ�����
xlabel('\tau (s)','fontsize',15);
ylabel('\sigma(\tau)','fontsize',15,'Rotation',90);
% title('Allan deviation','fontsize',15);
grid on
axis equal 
