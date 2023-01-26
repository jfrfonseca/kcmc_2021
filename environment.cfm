AWSTemplateFormatVersion: 2010-09-09
Description: Environment required to run the KCMC Heuristic project

Parameters:

  VPC:
    # Choose a VPC. You can get the default for your account with the command:
    # aws ec2 describe-vpcs \
    #     --filters Name=isDefault,Values=true \
    #     --query 'Vpcs[*].VpcId' \
    #     --output text)
    Type: String
    Default: "vpc-0091cb67"

  SUBNET:
    # Choose and set a SubNet. You can see available SubNets with the command:
    # aws ec2 describe-subnets \
    #     --filters Name=state,Values=available
    #     --filters Name=default-for-az,Values=true
    #     --filters Name=vpc-id,Values=${VPC_ID}
    Type: String
    Default: "subnet-d0658e8b"

  Prefix:
    Type: String
    Default: "kcmc"

  WorkerImageTag:
    Type: String
    Default: "latest"

  WorkerCPU:
    Type: String
    # vCPU for the Worker
    # 256 (.25 vCPU) - Available memory values: 0.5GB, 1GB, 2GB
    # 512 (.5 vCPU) - Available memory values: 1GB, 2GB, 3GB, 4GB
    # 1024 (1 vCPU) - Available memory values: 2GB, 3GB, 4GB, 5GB, 6GB, 7GB, 8GB
    # 2048 (2 vCPU) - Available memory values: Between 4GB and 16GB in 1GB increments
    # 4096 (4 vCPU) - Available memory values: Between 8GB and 30GB in 1GB increments
    Default: "2048"

  WorkerMemory:
    Type: String
    # Memory for the Worker
    # 0.5GB, 1GB, 2GB - Available cpu values: 256 (.25 vCPU)
    # 1GB, 2GB, 3GB, 4GB - Available cpu values: 512 (.5 vCPU)
    # 2GB, 3GB, 4GB, 5GB, 6GB, 7GB, 8GB - Available cpu values: 1024 (1 vCPU)
    # Between 4GB and 16GB in 1GB increments - Available cpu values: 2048 (2 vCPU)
    # Between 8GB and 30GB in 1GB increments - Available cpu values: 4096 (4 vCPU)
    Default: "16GB"

Resources:

  WorkerECRRepo:
    Type: AWS::ECR::Repository
    Properties:
      RepositoryName: !Join ['_', [!Ref Prefix]]
      Tags:
        - Key: CostCenter
          Value: kcmc
        - Key: Name
          Value: !Join ['', [!Ref Prefix, WorkerECRRepo]]

  WorkerLogGroup:
    Type: AWS::Logs::LogGroup
    Properties:
      LogGroupName: !Join ['', [/, !Ref Prefix]]
      RetentionInDays: 7
      Tags:
        - Key: CostCenter
          Value: kcmc
        - Key: Name
          Value: !Join ['', [!Ref Prefix, WorkerLogGroup]]

  WorkerExecutionRole:
    Type: AWS::IAM::Role
    Properties:
      RoleName: !Join ['', [!Ref Prefix, WorkerExecutionRole]]
      AssumeRolePolicyDocument:
        Statement:
          - Effect: Allow
            Principal:
              Service: ecs-tasks.amazonaws.com
            Action: 'sts:AssumeRole'
      ManagedPolicyArns:
        - 'arn:aws:iam::aws:policy/service-role/AmazonECSTaskExecutionRolePolicy'
      Tags:
        - Key: CostCenter
          Value: kcmc
        - Key: Name
          Value: !Join ['', [!Ref Prefix, WorkerExecutionRole]]

  WorkerSecurityGroup:
    Type: AWS::EC2::SecurityGroup
    Properties:
      GroupName: !Join ['', [!Ref Prefix, WorkerSG]]
      GroupDescription: "Security Group For the Fargate Workers"
      VpcId: !Ref VPC
      Tags:
        - Key: CostCenter
          Value: kcmc
        - Key: Name
          Value: !Join ['', [!Ref Prefix, WorkerSG]]

  WorkerSecurityGroupIngress:
    Type: AWS::EC2::SecurityGroupIngress
    DependsOn:
      - WorkerSecurityGroup
    Properties:
      GroupId: !GetAtt WorkerSecurityGroup.GroupId
      IpProtocol: -1
      FromPort: 0
      ToPort: 65535
      SourceSecurityGroupId: !GetAtt WorkerSecurityGroup.GroupId

  WorkerTaskExecutionRole:
    Type: AWS::IAM::Role
    Properties:
      RoleName: !Join ['-', [!Ref Prefix, WorkerTaskExecutionRole]]
      AssumeRolePolicyDocument:
        Statement:
          - Effect: Allow
            Principal:
              Service: ecs-tasks.amazonaws.com
            Action: 'sts:AssumeRole'
      Policies:
        - PolicyName: "WorkerTaskExecutionRolePolicy"
          PolicyDocument:
            Version: "2012-10-17"
            Statement:
              - Sid: "WorkerTaskExecutionRolePolicy"
                Effect: Allow
                Resource: "*"
                Action:
                  - logs:*
                  - ecr:*
                  - secretsmanager:*
                  - s3:*
      Tags:
        - Key: CostCenter
          Value: kcmc
        - Key: Name
          Value: !Join ['', [!Ref Prefix, WorkerTaskExecutionRole]]

  WorkerCluster:
    Type: AWS::ECS::Cluster
    Properties:
      ClusterName: !Join ['', [!Ref Prefix, WorkerCluster]]
      ClusterSettings:
        - Name: containerInsights
          Value: disabled

  WorkerTask:
    Type: AWS::ECS::TaskDefinition
    Properties:
      RequiresCompatibilities:
        - "FARGATE"
      Family: !Join ['', [!Ref Prefix, WorkerTask]]
      ExecutionRoleArn: !GetAtt WorkerExecutionRole.Arn
      TaskRoleArn: !GetAtt WorkerTaskExecutionRole.Arn
      NetworkMode: awsvpc
      Cpu: !Ref WorkerCPU
      Memory: !Ref WorkerMemory
      ContainerDefinitions:
        - Name: "worker"
          Image: !Join ['', [!GetAtt WorkerECRRepo.RepositoryUri, ":", !Ref WorkerImageTag]]
          Essential: true
          LogConfiguration:
            LogDriver: awslogs
            Options:
              awslogs-region: !Ref AWS::Region
              awslogs-group: !Ref WorkerLogGroup
              awslogs-stream-prefix: worker

Outputs:
  VPC:
    Description: "Worker VPC"
    Value:
      !Ref VPC
  Subnet:
    Description: "Worker SUBNET"
    Value:
      !Ref SUBNET
  SecurityGroup:
    Description: "Worker Security Group"
    Value:
      !GetAtt WorkerSecurityGroup.GroupId
  ImageRepo:
    Description: "Worker Image ECR Repository"
    Value:
      !Join ['', [!GetAtt WorkerECRRepo.RepositoryUri, ':', !Ref WorkerImageTag]]
